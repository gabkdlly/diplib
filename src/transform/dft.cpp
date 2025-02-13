/*
 * (c)2017-2022, Cris Luengo, Erik Schuitema
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "diplib.h"
#include "diplib/dft.h"
#include "diplib/private/robin_map.h"

#ifdef DIP_CONFIG_HAS_FFTW

#ifdef _WIN32
#define NOMINMAX // windows.h must not define min() and max(), which conflict with std::min() and std::max()
#endif
#include "fftw3api.h"

#else // DIP_CONFIG_HAS_FFTW

#if defined(__GNUG__) || defined(__clang__)
// Temporarily turn off -Wconversion, PocketFFT otherwise produces two warnings
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#define POCKETFFT_NO_VECTORS // is faster on my M1 Mac!
#define POCKETFFT_NO_MULTITHREADING
#include "pocketfft_hdronly.h"

namespace pocketfft { // we want these in the main namespace
using detail::cmplx;
using detail::pocketfft_c;
using detail::pocketfft_r;
}

#if defined(__GNUG__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#endif // DIP_CONFIG_HAS_FFTW


namespace dip {


#ifdef DIP_CONFIG_HAS_FFTW

dip::uint const maximumDFTSize = static_cast< dip::uint >( std::numeric_limits< int >::max() );
extern bool const usingFFTW = true;

template< typename T >
using complex = typename fftwapidef< T >::complex;

template< typename T >
using CPlan = typename fftwapidef< T >::plan;

template< typename T >
using RPlan = typename fftwapidef< T >::plan;

#else // DIP_CONFIG_HAS_FFTW

dip::uint const maximumDFTSize = std::numeric_limits< dip::uint >::max();
extern bool const usingFFTW = false;

template< typename T >
using complex = pocketfft::cmplx< T >;

template< typename T >
using CPlan = pocketfft::pocketfft_c< T >;

template< typename T >
using RPlan = pocketfft::pocketfft_r< T >;

namespace {

// This is a container that holds plans, and maintains a use count. Plans in use cannot be
// deleted.
//
// This code is developed based on pocketfft::detail::get_plan<>(), but modified beyond recognition.
//
// PlanType is CPlan< T > or RPlan< T >, T is dip::sfloat or dip::dfloat. There are 4 caches.
template< typename PlanType >
PlanType* PlanCache( dip::uint length, bool free = false ) {
   struct Data {
      dip::uint use_count;
      dip::uint last_access;
      std::unique_ptr< PlanType > plan;
   };
   static tsl::robin_map< dip::uint, Data > cache;
   static dip::uint access_counter = 0;
   constexpr dip::uint maxSize = 30; // If there are more than maxSize plans, delete ones not in use.

   if( free ) {
      auto it = cache.find( length );
      if( it != cache.end()) {
         auto& data = it.value();
         if( data.use_count > 0 ) {
            --( data.use_count );
            //std::cout << "Freeing cache element for length = " << length;
            //std::cout << ", current use count = " << data.use_count << '\n';
         }
      }
      return nullptr;
   }

   if( access_counter == std::numeric_limits< dip::uint >::max()) {
      // This could happen... but really???
      access_counter = 1;
      // Update `last_access` from all plans to be 0, so that things make sense again.
      for( auto& p : cache ) {
         const_cast< Data& >( p.second ).last_access = 0; // Why is the data const-qualified here?
      }
   }
   auto it = cache.find( length );
   if( it != cache.end()) {
      // Update
      //std::cout << "Re-using cache element for length = " << length;
      auto& data = it.value();
      ++( data.use_count );
      data.last_access = ++access_counter;
      //std::cout << ", current use count = " << data.use_count << '\n';
      return data.plan.get();
   }
   // Not found
   if( cache.size() >= maxSize ) {
      //std::cout << "Cache is getting big... ";
      // Delete one cache element that is not in use, if possible
      dip::uint acc = access_counter;
      dip::uint len = 0;
      for( auto& p : cache ) {
         if( p.second.use_count == 0 ) { // not in use
            if( p.second.last_access < acc ) {
               acc = p.second.last_access;
               len = p.second.plan->length();
            }
         }
      }
      // If there is something to delete, we will
      if( len > 0 ) {
         //std::cout << "Deleting cache element for length = " << len << '\n';
         cache.erase( len );
      } else {
         //std::cout << "Nothing found to delete\n";
      }
   }
   //std::cout << "Creating cache element for length = " << length << '\n';
   Data data{ 1, ++access_counter, std::make_unique< PlanType >( length ) };
   cache.insert( { length, std::move( data ) } );
   return cache[ length ].plan.get();
}

} // namespace

#endif // DIP_CONFIG_HAS_FFTW


//--- dip::DFT implementation ---


template< typename T >
void DFT< T >::Initialize( dip::uint size, bool inverse, Option::DFTOptions options ) {
   Destroy();
   nfft_ = size;
   inverse_ = inverse;
   options_ = options;
#ifdef DIP_CONFIG_HAS_FFTW
   int sign = inverse ? FFTW_BACKWARD : FFTW_FORWARD;
   unsigned flags = FFTW_MEASURE; // FFTW_MEASURE is almost always faster than FFTW_ESTIMATE, only for very trivial sizes it's not.
   if( !options_.Contains( Option::DFTOption::Aligned )) {
      flags |= FFTW_UNALIGNED;
   }
   AlignedBuffer inBuffer( size * sizeof( complex< T > )); // allocate temporary arrays just for planning...
   complex< T >* in = reinterpret_cast< complex< T >* >( inBuffer.data() );
   if( options_.Contains( Option::DFTOption::InPlace )) {
      plan_ = fftwapidef< T >::plan_dft_1d( static_cast< int >( size ), in, in, sign, flags );
   } else {
      flags |= options_.Contains( Option::DFTOption::TrashInput ) ? FFTW_DESTROY_INPUT : FFTW_PRESERVE_INPUT;
      AlignedBuffer outBuffer( size * sizeof( complex< T > )); // allocate temporary arrays just for planning...
      complex< T >* out = reinterpret_cast< complex< T >* >( outBuffer.data() );
      plan_ = fftwapidef< T >::plan_dft_1d( static_cast< int >( size ), in, out, sign, flags );
   }
#else // DIP_CONFIG_HAS_FFTW
   ( void ) options; // parameter ignored
   plan_ = PlanCache< CPlan< T >>( size );
#endif // DIP_CONFIG_HAS_FFTW
}

template void DFT< dfloat >::Initialize( dip::uint, bool, Option::DFTOptions );
template void DFT< sfloat >::Initialize( dip::uint, bool, Option::DFTOptions );


template< typename T >
void DFT< T >::Apply(
      std::complex< T >* source,
      std::complex< T >* destination,
      T scale
) const {
   DIP_THROW_IF( !plan_, "No plan defined." );
#ifdef DIP_CONFIG_HAS_FFTW
   DIP_ASSERT(( source == destination) ^ !options_.Contains( Option::DFTOption::InPlace ));
   fftwapidef< T >::execute_dft( static_cast< CPlan< T >>( plan_ ),
                                 reinterpret_cast< complex< T >* >( source ),
                                 reinterpret_cast< complex< T >* >( destination ));
   if( scale != 1.0 ) {
      for( std::complex< T >* ptr = destination; ptr < destination + nfft_; ++ptr ) {
         *ptr *= scale;
      }
   }
#else // DIP_CONFIG_HAS_FFTW
   if( destination != source ) {
      std::copy_n( source, nfft_, destination );
   }
   static_cast< CPlan< T >* >( plan_ )->exec( reinterpret_cast< complex< T >* >( destination ), scale, !inverse_ );
#endif // DIP_CONFIG_HAS_FFTW
}

template void DFT< dfloat >::Apply( std::complex< dfloat >*, std::complex< dfloat >*, dfloat ) const;
template void DFT< sfloat >::Apply( std::complex< sfloat >*, std::complex< sfloat >*, sfloat ) const;


template< typename T >
void DFT< T >::Destroy() {
   if( plan_ ) {
#ifdef DIP_CONFIG_HAS_FFTW
      fftwapidef< T >::destroy_plan( static_cast< CPlan< T >>( plan_ ));
#else // DIP_CONFIG_HAS_FFTW
      PlanCache< CPlan< T >>( nfft_, true );
#endif // DIP_CONFIG_HAS_FFTW
      plan_ = nullptr;
   }
}

template void DFT< dfloat >::Destroy();
template void DFT< sfloat >::Destroy();


// --- dip::RDFT implementation ---


template< typename T >
void RDFT< T >::Initialize( dip::uint size, bool inverse, Option::DFTOptions options ) {
   Destroy();
   nfft_ = size;
   inverse_ = inverse;
   options_ = options;
#ifdef DIP_CONFIG_HAS_FFTW
   unsigned flags = FFTW_MEASURE; // FFTW_MEASURE is almost always faster than FFTW_ESTIMATE, only for very trivial sizes it's not.
   if( !options_.Contains( Option::DFTOption::Aligned )) {
      flags |= FFTW_UNALIGNED;
   }
   AlignedBuffer firstBuffer(( size / 2 + 1 ) * 2 * sizeof( T )); // allocate temporary arrays just for planning...
   if( options_.Contains( Option::DFTOption::InPlace )) {
      T* real = reinterpret_cast< T* >( firstBuffer.data() );
      complex< T >* comp = reinterpret_cast< complex< T >* >( firstBuffer.data() );
      if( inverse_ ) {
         plan_ = fftwapidef< T >::plan_dft_c2r_1d( static_cast< int >( size ), comp, real, flags );
      } else {
         plan_ = fftwapidef< T >::plan_dft_r2c_1d( static_cast< int >( size ), real, comp, flags );
      }
   } else {
      flags |= options_.Contains( Option::DFTOption::TrashInput ) ? FFTW_DESTROY_INPUT : FFTW_PRESERVE_INPUT;
      AlignedBuffer secondBuffer( size * sizeof( T )); // allocate temporary arrays just for planning...
      T* real = reinterpret_cast< T* >( secondBuffer.data() );
      complex< T >* comp = reinterpret_cast< complex< T >* >( firstBuffer.data() );
      if( inverse_ ) {
         plan_ = fftwapidef< T >::plan_dft_c2r_1d( static_cast< int >( size ), comp, real, flags );
      } else {
         plan_ = fftwapidef< T >::plan_dft_r2c_1d( static_cast< int >( size ), real, comp, flags );
      }
   }
#else // DIP_CONFIG_HAS_FFTW
   plan_ = PlanCache< RPlan< T >>( size );
#endif // DIP_CONFIG_HAS_FFTW
}

template void RDFT< dfloat >::Initialize( dip::uint, bool, Option::DFTOptions );
template void RDFT< sfloat >::Initialize( dip::uint, bool, Option::DFTOptions );


template< typename T >
void RDFT< T >::Apply(
      T* source,
      T* destination,
      T scale
) const {
   DIP_THROW_IF( !plan_, "No plan defined." );
#ifdef DIP_CONFIG_HAS_FFTW
   dip::uint len = nfft_;
   if( inverse_ ) {
      fftwapidef< T >::execute_dft_c2r( static_cast< RPlan< T >>( plan_ ),
                                        reinterpret_cast< complex< T >* >( source ),
                                        destination );
   } else {
      fftwapidef< T >::execute_dft_r2c( static_cast< RPlan< T >>( plan_ ),
                                        source,
                                        reinterpret_cast< complex< T >* >( destination ));
      len = 2 * ( nfft_ / 2 + 1 );
   }
   if( scale != 1.0 ) {
      for( T* ptr = destination; ptr < destination + len; ++ptr ) {
         *ptr *= scale;
      }
   }
#else // DIP_CONFIG_HAS_FFTW
   if( inverse_ ) {
      // Coerce the complex input data into the form expected by PocketFFT: the first (and last if nfft_ is even) elements only have a real value
      destination[ 0 ] = source[ 0 ];
      std::copy_n( source + 2, nfft_ - 1, destination + 1 ); // should work if source==destination
   } else {
      if( source != destination ) {
         // Copy the real input data into the complex output array, which should always have enough space
         std::copy_n( source, nfft_, destination );
      }
   }
   static_cast< RPlan< T >* >( plan_ )->exec( destination, scale, !inverse_ );
   if( !inverse_ ) {
      // Translate the complex output into proper complex numbers.
      dip::uint n = 2 * ( nfft_ / 2 + 1 ); // number of values on output array
      destination[ n - 1 ] = 0;
      if(( nfft_ & 1u ) == 0 ) {
         --n;
      }
      std::copy_backward( destination + 1, destination + nfft_, destination + n );
      destination[ 1 ] = 0;
   }
#endif // DIP_CONFIG_HAS_FFTW
}

template void RDFT< dfloat >::Apply( dfloat*, dfloat*, dfloat ) const;
template void RDFT< sfloat >::Apply( sfloat*, sfloat*, sfloat ) const;


template< typename T >
void RDFT< T >::Destroy() {
   if( plan_ ) {
#ifdef DIP_CONFIG_HAS_FFTW
      fftwapidef< T >::destroy_plan( static_cast< RPlan< T >>( plan_ ));
#else // DIP_CONFIG_HAS_FFTW
      PlanCache< RPlan< T >>( nfft_, true );
#endif // DIP_CONFIG_HAS_FFTW
      plan_ = nullptr;
   }
}

template void RDFT< dfloat >::Destroy();
template void RDFT< sfloat >::Destroy();


// --- Support functions ---


namespace {

constexpr unsigned int optimalDFTSizeTab[] = {
      1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 25, 27, 30, 32, 36, 40, 45, 48,
      50, 54, 60, 64, 72, 75, 80, 81, 90, 96, 100, 108, 120, 125, 128, 135, 144, 150, 160,
      162, 180, 192, 200, 216, 225, 240, 243, 250, 256, 270, 288, 300, 320, 324, 360, 375,
      384, 400, 405, 432, 450, 480, 486, 500, 512, 540, 576, 600, 625, 640, 648, 675, 720,
      729, 750, 768, 800, 810, 864, 900, 960, 972, 1000, 1024, 1080, 1125, 1152, 1200,
      1215, 1250, 1280, 1296, 1350, 1440, 1458, 1500, 1536, 1600, 1620, 1728, 1800, 1875,
      1920, 1944, 2000, 2025, 2048, 2160, 2187, 2250, 2304, 2400, 2430, 2500, 2560, 2592,
      2700, 2880, 2916, 3000, 3072, 3125, 3200, 3240, 3375, 3456, 3600, 3645, 3750, 3840,
      3888, 4000, 4050, 4096, 4320, 4374, 4500, 4608, 4800, 4860, 5000, 5120, 5184, 5400,
      5625, 5760, 5832, 6000, 6075, 6144, 6250, 6400, 6480, 6561, 6750, 6912, 7200, 7290,
      7500, 7680, 7776, 8000, 8100, 8192, 8640, 8748, 9000, 9216, 9375, 9600, 9720, 10000,
      10125, 10240, 10368, 10800, 10935, 11250, 11520, 11664, 12000, 12150, 12288, 12500,
      12800, 12960, 13122, 13500, 13824, 14400, 14580, 15000, 15360, 15552, 15625, 16000,
      16200, 16384, 16875, 17280, 17496, 18000, 18225, 18432, 18750, 19200, 19440, 19683,
      20000, 20250, 20480, 20736, 21600, 21870, 22500, 23040, 23328, 24000, 24300, 24576,
      25000, 25600, 25920, 26244, 27000, 27648, 28125, 28800, 29160, 30000, 30375, 30720,
      31104, 31250, 32000, 32400, 32768, 32805, 33750, 34560, 34992, 36000, 36450, 36864,
      37500, 38400, 38880, 39366, 40000, 40500, 40960, 41472, 43200, 43740, 45000, 46080,
      46656, 46875, 48000, 48600, 49152, 50000, 50625, 51200, 51840, 52488, 54000, 54675,
      55296, 56250, 57600, 58320, 59049, 60000, 60750, 61440, 62208, 62500, 64000, 64800,
      65536, 65610, 67500, 69120, 69984, 72000, 72900, 73728, 75000, 76800, 77760, 78125,
      78732, 80000, 81000, 81920, 82944, 84375, 86400, 87480, 90000, 91125, 92160, 93312,
      93750, 96000, 97200, 98304, 98415, 100000, 101250, 102400, 103680, 104976, 108000,
      109350, 110592, 112500, 115200, 116640, 118098, 120000, 121500, 122880, 124416, 125000,
      128000, 129600, 131072, 131220, 135000, 138240, 139968, 140625, 144000, 145800, 147456,
      150000, 151875, 153600, 155520, 156250, 157464, 160000, 162000, 163840, 164025, 165888,
      168750, 172800, 174960, 177147, 180000, 182250, 184320, 186624, 187500, 192000, 194400,
      196608, 196830, 200000, 202500, 204800, 207360, 209952, 216000, 218700, 221184, 225000,
      230400, 233280, 234375, 236196, 240000, 243000, 245760, 248832, 250000, 253125, 256000,
      259200, 262144, 262440, 270000, 273375, 276480, 279936, 281250, 288000, 291600, 294912,
      295245, 300000, 303750, 307200, 311040, 312500, 314928, 320000, 324000, 327680, 328050,
      331776, 337500, 345600, 349920, 354294, 360000, 364500, 368640, 373248, 375000, 384000,
      388800, 390625, 393216, 393660, 400000, 405000, 409600, 414720, 419904, 421875, 432000,
      437400, 442368, 450000, 455625, 460800, 466560, 468750, 472392, 480000, 486000, 491520,
      492075, 497664, 500000, 506250, 512000, 518400, 524288, 524880, 531441, 540000, 546750,
      552960, 559872, 562500, 576000, 583200, 589824, 590490, 600000, 607500, 614400, 622080,
      625000, 629856, 640000, 648000, 655360, 656100, 663552, 675000, 691200, 699840, 703125,
      708588, 720000, 729000, 737280, 746496, 750000, 759375, 768000, 777600, 781250, 786432,
      787320, 800000, 810000, 819200, 820125, 829440, 839808, 843750, 864000, 874800, 884736,
      885735, 900000, 911250, 921600, 933120, 937500, 944784, 960000, 972000, 983040, 984150,
      995328, 1000000, 1012500, 1024000, 1036800, 1048576, 1049760, 1062882, 1080000, 1093500,
      1105920, 1119744, 1125000, 1152000, 1166400, 1171875, 1179648, 1180980, 1200000,
      1215000, 1228800, 1244160, 1250000, 1259712, 1265625, 1280000, 1296000, 1310720,
      1312200, 1327104, 1350000, 1366875, 1382400, 1399680, 1406250, 1417176, 1440000,
      1458000, 1474560, 1476225, 1492992, 1500000, 1518750, 1536000, 1555200, 1562500,
      1572864, 1574640, 1594323, 1600000, 1620000, 1638400, 1640250, 1658880, 1679616,
      1687500, 1728000, 1749600, 1769472, 1771470, 1800000, 1822500, 1843200, 1866240,
      1875000, 1889568, 1920000, 1944000, 1953125, 1966080, 1968300, 1990656, 2000000,
      2025000, 2048000, 2073600, 2097152, 2099520, 2109375, 2125764, 2160000, 2187000,
      2211840, 2239488, 2250000, 2278125, 2304000, 2332800, 2343750, 2359296, 2361960,
      2400000, 2430000, 2457600, 2460375, 2488320, 2500000, 2519424, 2531250, 2560000,
      2592000, 2621440, 2624400, 2654208, 2657205, 2700000, 2733750, 2764800, 2799360,
      2812500, 2834352, 2880000, 2916000, 2949120, 2952450, 2985984, 3000000, 3037500,
      3072000, 3110400, 3125000, 3145728, 3149280, 3188646, 3200000, 3240000, 3276800,
      3280500, 3317760, 3359232, 3375000, 3456000, 3499200, 3515625, 3538944, 3542940,
      3600000, 3645000, 3686400, 3732480, 3750000, 3779136, 3796875, 3840000, 3888000,
      3906250, 3932160, 3936600, 3981312, 4000000, 4050000, 4096000, 4100625, 4147200,
      4194304, 4199040, 4218750, 4251528, 4320000, 4374000, 4423680, 4428675, 4478976,
      4500000, 4556250, 4608000, 4665600, 4687500, 4718592, 4723920, 4782969, 4800000,
      4860000, 4915200, 4920750, 4976640, 5000000, 5038848, 5062500, 5120000, 5184000,
      5242880, 5248800, 5308416, 5314410, 5400000, 5467500, 5529600, 5598720, 5625000,
      5668704, 5760000, 5832000, 5859375, 5898240, 5904900, 5971968, 6000000, 6075000,
      6144000, 6220800, 6250000, 6291456, 6298560, 6328125, 6377292, 6400000, 6480000,
      6553600, 6561000, 6635520, 6718464, 6750000, 6834375, 6912000, 6998400, 7031250,
      7077888, 7085880, 7200000, 7290000, 7372800, 7381125, 7464960, 7500000, 7558272,
      7593750, 7680000, 7776000, 7812500, 7864320, 7873200, 7962624, 7971615, 8000000,
      8100000, 8192000, 8201250, 8294400, 8388608, 8398080, 8437500, 8503056, 8640000,
      8748000, 8847360, 8857350, 8957952, 9000000, 9112500, 9216000, 9331200, 9375000,
      9437184, 9447840, 9565938, 9600000, 9720000, 9765625, 9830400, 9841500, 9953280,
      10000000, 10077696, 10125000, 10240000, 10368000, 10485760, 10497600, 10546875, 10616832,
      10628820, 10800000, 10935000, 11059200, 11197440, 11250000, 11337408, 11390625, 11520000,
      11664000, 11718750, 11796480, 11809800, 11943936, 12000000, 12150000, 12288000, 12301875,
      12441600, 12500000, 12582912, 12597120, 12656250, 12754584, 12800000, 12960000, 13107200,
      13122000, 13271040, 13286025, 13436928, 13500000, 13668750, 13824000, 13996800, 14062500,
      14155776, 14171760, 14400000, 14580000, 14745600, 14762250, 14929920, 15000000, 15116544,
      15187500, 15360000, 15552000, 15625000, 15728640, 15746400, 15925248, 15943230, 16000000,
      16200000, 16384000, 16402500, 16588800, 16777216, 16796160, 16875000, 17006112, 17280000,
      17496000, 17578125, 17694720, 17714700, 17915904, 18000000, 18225000, 18432000, 18662400,
      18750000, 18874368, 18895680, 18984375, 19131876, 19200000, 19440000, 19531250, 19660800,
      19683000, 19906560, 20000000, 20155392, 20250000, 20480000, 20503125, 20736000, 20971520,
      20995200, 21093750, 21233664, 21257640, 21600000, 21870000, 22118400, 22143375, 22394880,
      22500000, 22674816, 22781250, 23040000, 23328000, 23437500, 23592960, 23619600, 23887872,
      23914845, 24000000, 24300000, 24576000, 24603750, 24883200, 25000000, 25165824, 25194240,
      25312500, 25509168, 25600000, 25920000, 26214400, 26244000, 26542080, 26572050, 26873856,
      27000000, 27337500, 27648000, 27993600, 28125000, 28311552, 28343520, 28800000, 29160000,
      29296875, 29491200, 29524500, 29859840, 30000000, 30233088, 30375000, 30720000, 31104000,
      31250000, 31457280, 31492800, 31640625, 31850496, 31886460, 32000000, 32400000, 32768000,
      32805000, 33177600, 33554432, 33592320, 33750000, 34012224, 34171875, 34560000, 34992000,
      35156250, 35389440, 35429400, 35831808, 36000000, 36450000, 36864000, 36905625, 37324800,
      37500000, 37748736, 37791360, 37968750, 38263752, 38400000, 38880000, 39062500, 39321600,
      39366000, 39813120, 39858075, 40000000, 40310784, 40500000, 40960000, 41006250, 41472000,
      41943040, 41990400, 42187500, 42467328, 42515280, 43200000, 43740000, 44236800, 44286750,
      44789760, 45000000, 45349632, 45562500, 46080000, 46656000, 46875000, 47185920, 47239200,
      47775744, 47829690, 48000000, 48600000, 48828125, 49152000, 49207500, 49766400, 50000000,
      50331648, 50388480, 50625000, 51018336, 51200000, 51840000, 52428800, 52488000, 52734375,
      53084160, 53144100, 53747712, 54000000, 54675000, 55296000, 55987200, 56250000, 56623104,
      56687040, 56953125, 57600000, 58320000, 58593750, 58982400, 59049000, 59719680, 60000000,
      60466176, 60750000, 61440000, 61509375, 62208000, 62500000, 62914560, 62985600, 63281250,
      63700992, 63772920, 64000000, 64800000, 65536000, 65610000, 66355200, 66430125, 67108864,
      67184640, 67500000, 68024448, 68343750, 69120000, 69984000, 70312500, 70778880, 70858800,
      71663616, 72000000, 72900000, 73728000, 73811250, 74649600, 75000000, 75497472, 75582720,
      75937500, 76527504, 76800000, 77760000, 78125000, 78643200, 78732000, 79626240, 79716150,
      80000000, 80621568, 81000000, 81920000, 82012500, 82944000, 83886080, 83980800, 84375000,
      84934656, 85030560, 86400000, 87480000, 87890625, 88473600, 88573500, 89579520, 90000000,
      90699264, 91125000, 92160000, 93312000, 93750000, 94371840, 94478400, 94921875, 95551488,
      95659380, 96000000, 97200000, 97656250, 98304000, 98415000, 99532800, 100000000,
      100663296, 100776960, 101250000, 102036672, 102400000, 102515625, 103680000, 104857600,
      104976000, 105468750, 106168320, 106288200, 107495424, 108000000, 109350000, 110592000,
      110716875, 111974400, 112500000, 113246208, 113374080, 113906250, 115200000, 116640000,
      117187500, 117964800, 118098000, 119439360, 119574225, 120000000, 120932352, 121500000,
      122880000, 123018750, 124416000, 125000000, 125829120, 125971200, 126562500, 127401984,
      127545840, 128000000, 129600000, 131072000, 131220000, 132710400, 132860250, 134217728,
      134369280, 135000000, 136048896, 136687500, 138240000, 139968000, 140625000, 141557760,
      141717600, 143327232, 144000000, 145800000, 146484375, 147456000, 147622500, 149299200,
      150000000, 150994944, 151165440, 151875000, 153055008, 153600000, 155520000, 156250000,
      157286400, 157464000, 158203125, 159252480, 159432300, 160000000, 161243136, 162000000,
      163840000, 164025000, 165888000, 167772160, 167961600, 168750000, 169869312, 170061120,
      170859375, 172800000, 174960000, 175781250, 176947200, 177147000, 179159040, 180000000,
      181398528, 182250000, 184320000, 184528125, 186624000, 187500000, 188743680, 188956800,
      189843750, 191102976, 191318760, 192000000, 194400000, 195312500, 196608000, 196830000,
      199065600, 199290375, 200000000, 201326592, 201553920, 202500000, 204073344, 204800000,
      205031250, 207360000, 209715200, 209952000, 210937500, 212336640, 212576400, 214990848,
      216000000, 218700000, 221184000, 221433750, 223948800, 225000000, 226492416, 226748160,
      227812500, 230400000, 233280000, 234375000, 235929600, 236196000, 238878720, 239148450,
      240000000, 241864704, 243000000, 244140625, 245760000, 246037500, 248832000, 250000000,
      251658240, 251942400, 253125000, 254803968, 255091680, 256000000, 259200000, 262144000,
      262440000, 263671875, 265420800, 265720500, 268435456, 268738560, 270000000, 272097792,
      273375000, 276480000, 279936000, 281250000, 283115520, 283435200, 284765625, 286654464,
      288000000, 291600000, 292968750, 294912000, 295245000, 298598400, 300000000, 301989888,
      302330880, 303750000, 306110016, 307200000, 307546875, 311040000, 312500000, 314572800,
      314928000, 316406250, 318504960, 318864600, 320000000, 322486272, 324000000, 327680000,
      328050000, 331776000, 332150625, 335544320, 335923200, 337500000, 339738624, 340122240,
      341718750, 345600000, 349920000, 351562500, 353894400, 354294000, 358318080, 360000000,
      362797056, 364500000, 368640000, 369056250, 373248000, 375000000, 377487360, 377913600,
      379687500, 382205952, 382637520, 384000000, 388800000, 390625000, 393216000, 393660000,
      398131200, 398580750, 400000000, 402653184, 403107840, 405000000, 408146688, 409600000,
      410062500, 414720000, 419430400, 419904000, 421875000, 424673280, 425152800, 429981696,
      432000000, 437400000, 439453125, 442368000, 442867500, 447897600, 450000000, 452984832,
      453496320, 455625000, 460800000, 466560000, 468750000, 471859200, 472392000, 474609375,
      477757440, 478296900, 480000000, 483729408, 486000000, 488281250, 491520000, 492075000,
      497664000, 500000000, 503316480, 503884800, 506250000, 509607936, 510183360, 512000000,
      512578125, 518400000, 524288000, 524880000, 527343750, 530841600, 531441000, 536870912,
      537477120, 540000000, 544195584, 546750000, 552960000, 553584375, 559872000, 562500000,
      566231040, 566870400, 569531250, 573308928, 576000000, 583200000, 585937500, 589824000,
      590490000, 597196800, 597871125, 600000000, 603979776, 604661760, 607500000, 612220032,
      614400000, 615093750, 622080000, 625000000, 629145600, 629856000, 632812500, 637009920,
      637729200, 640000000, 644972544, 648000000, 655360000, 656100000, 663552000, 664301250,
      671088640, 671846400, 675000000, 679477248, 680244480, 683437500, 691200000, 699840000,
      703125000, 707788800, 708588000, 716636160, 720000000, 725594112, 729000000, 732421875,
      737280000, 738112500, 746496000, 750000000, 754974720, 755827200, 759375000, 764411904,
      765275040, 768000000, 777600000, 781250000, 786432000, 787320000, 791015625, 796262400,
      797161500, 800000000, 805306368, 806215680, 810000000, 816293376, 819200000, 820125000,
      829440000, 838860800, 839808000, 843750000, 849346560, 850305600, 854296875, 859963392,
      864000000, 874800000, 878906250, 884736000, 885735000, 895795200, 900000000, 905969664,
      906992640, 911250000, 921600000, 922640625, 933120000, 937500000, 943718400, 944784000,
      949218750, 955514880, 956593800, 960000000, 967458816, 972000000, 976562500, 983040000,
      984150000, 995328000, 996451875, 1000000000, 1006632960, 1007769600, 1012500000,
      1019215872, 1020366720, 1024000000, 1025156250, 1036800000, 1048576000, 1049760000,
      1054687500, 1061683200, 1062882000, 1073741824, 1074954240, 1080000000, 1088391168,
      1093500000, 1105920000, 1107168750, 1119744000, 1125000000, 1132462080, 1133740800,
      1139062500, 1146617856, 1152000000, 1166400000, 1171875000, 1179648000, 1180980000,
      1194393600, 1195742250, 1200000000, 1207959552, 1209323520, 1215000000, 1220703125,
      1224440064, 1228800000, 1230187500, 1244160000, 1250000000, 1258291200, 1259712000,
      1265625000, 1274019840, 1275458400, 1280000000, 1289945088, 1296000000, 1310720000,
      1312200000, 1318359375, 1327104000, 1328602500, 1342177280, 1343692800, 1350000000,
      1358954496, 1360488960, 1366875000, 1382400000, 1399680000, 1406250000, 1415577600,
      1417176000, 1423828125, 1433272320, 1440000000, 1451188224, 1458000000, 1464843750,
      1474560000, 1476225000, 1492992000, 1500000000, 1509949440, 1511654400, 1518750000,
      1528823808, 1530550080, 1536000000, 1537734375, 1555200000, 1562500000, 1572864000,
      1574640000, 1582031250, 1592524800, 1594323000, 1600000000, 1610612736, 1612431360,
      1620000000, 1632586752, 1638400000, 1640250000, 1658880000, 1660753125, 1677721600,
      1679616000, 1687500000, 1698693120, 1700611200, 1708593750, 1719926784, 1728000000,
      1749600000, 1757812500, 1769472000, 1771470000, 1791590400, 1800000000, 1811939328,
      1813985280, 1822500000, 1843200000, 1845281250, 1866240000, 1875000000, 1887436800,
      1889568000, 1898437500, 1911029760, 1913187600, 1920000000, 1934917632, 1944000000,
      1953125000, 1966080000, 1968300000, 1990656000, 1992903750, 2000000000, 2013265920,
      2015539200, 2025000000, 2038431744, 2040733440, 2048000000, 2050312500, 2073600000,
      2097152000, 2099520000, 2109375000, 2123366400, 2125764000
};

/*
The table above can be reproduced with the following MATLAB code:

list = int64([]);
maxint = 2^31 - 1;
max5 = floor(log(maxint)/log(5));
for n5 = 0:max5
   v5 = 5^n5;
   max3 = floor(log(maxint/v5)/log(3));
   for n3 = 0:max3
      v3 = v5 * 3^n3;
      max2 = floor(log(maxint/v3)/log(2));
      for n2 = 0:max2
         v2 = v3 * 2^n2;
         list(end+1) = round(v2);
      end
   end
end
list = sort(list);
*/

} // namespace

dip::uint GetOptimalDFTSize( dip::uint size0, bool larger ) {
   // TODO: This only handles sizes up to 2^31-1, DIPlib will currently refuse to do "nice" sizes for larger transforms.
   //       I don't think it matters a whole lot, it's not like we'd ever use images that large in the foreseeable future.
   dip::uint a = 0;
   dip::uint b = sizeof( optimalDFTSizeTab ) / sizeof( optimalDFTSizeTab[ 0 ] ) - 1;
   if(( size0 > optimalDFTSizeTab[ b ] ) || ( size0 == 0 )) {
      return 0; // 0 indicates an error condition -- of course we don't do DFT on an empty array!
   }
   if( larger ) {
      while( a < b ) {
         dip::uint c = ( a + b ) / 2;
         if( size0 <= optimalDFTSizeTab[ c ] ) {
            b = c;
         } else {
            a = c + 1;
         }
      }
   } else {
      while( a < b ) {
         dip::uint c = ( a + b + 1 ) / 2;
         if( size0 >= optimalDFTSizeTab[ c ] ) {
            a = c;
         } else {
            b = c - 1;
         }
      }
   }
   return optimalDFTSizeTab[ b ];
}


} // namespace dip


#ifdef DIP_CONFIG_ENABLE_DOCTEST
#include "doctest.h"
#include "diplib/random.h"

constexpr long double pi = 3.1415926535897932384626433832795029l;

template< typename T >
T test_DFT( dip::uint nfft, bool inverse ) {
   // Initialize
   dip::DFT< T > opts( nfft, inverse );
   // Create test data
   std::vector< std::complex< T >> inbuf( nfft );
   std::vector< std::complex< T >> outbuf( nfft );
   dip::Random random;
   for( dip::uint k = 0; k < nfft; ++k ) {
      inbuf[ k ] = std::complex< T >( static_cast< T >( random() ), static_cast< T >( random() )) / static_cast< T >( random.max() ) - T( 0.5 );
   }
   // Do the thing
   opts.Apply( inbuf.data(), outbuf.data(), T( 1 ));
   // Check
   long double totalpower = 0;
   long double difpower = 0;
   for( dip::uint k0 = 0; k0 < nfft; ++k0 ) {
      std::complex< long double > acc{ 0, 0 };
      long double phinc = ( inverse ? 2 : -2 ) * static_cast< long double >( k0 ) * pi / static_cast< long double >( nfft );
      for( dip::uint k1 = 0; k1 < nfft; ++k1 ) {
         acc += std::complex< long double >( inbuf[ k1 ] ) * std::exp( std::complex< long double >( 0, static_cast< long double >( k1 ) * phinc ));
      }
      totalpower += std::norm( acc );
      difpower += std::norm( acc - std::complex< long double >( outbuf[ k0 ] ));
   }
   return static_cast< T >( std::sqrt( difpower / totalpower )); // Root mean square error
}

DOCTEST_TEST_CASE("[DIPlib] testing the DFT class") {
   // Test forward (C2C) transform for a few different sizes that have all different radixes.
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 32, false )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 32, false )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 256, false )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 105, false )) == 0 ); // 3*5*7
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 154, false )) == 0 ); // 2*7*11
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 97, false )) == 0 ); // prime
   // Idem for inverse (C2C) transform
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 32, true )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 32, true )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 256, true )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 105, true )) == 0 ); // 3*5*7
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::dfloat >( 154, true )) == 0 ); // 2*7*11
   DOCTEST_CHECK( doctest::Approx( test_DFT< dip::sfloat >( 97, true )) == 0 ); // prime
}

template< typename T >
T test_RDFT( dip::uint nfft ) {
   // Initialize
   dip::RDFT< T > opts( nfft, false );
   dip::uint nOut = nfft / 2 + 1;
   // Create test data
   std::vector< T > inbuf( nfft );
   std::vector< std::complex< T >> outbuf( nOut + 1 );
   outbuf.back() = 1e6; // make sure we're not reading past where we're supposed to
   dip::Random random;
   for( dip::uint k = 0; k < nfft; ++k ) {
      inbuf[ k ] = static_cast< T >( random() ) / static_cast< T >( random.max() ) - T( 0.5 );
   }
   // Do the thing
   opts.Apply( inbuf.data(), reinterpret_cast< T* >( outbuf.data() ), T( 1 ));
   DIP_THROW_IF( outbuf.back() != T( 1e6 ), "dip::RDFT< T >::Apply wrote too many values!" );
   // Check
   long double totalpower = 0;
   long double difpower = 0;
   for( dip::uint k0 = 0; k0 < nOut; ++k0 ) {
      std::complex< long double > acc{ 0, 0 };
      long double phinc = -2 * static_cast< long double >( k0 ) * pi / static_cast< long double >( nfft );
      for( dip::uint k1 = 0; k1 < nfft; ++k1 ) {
         acc += std::complex< long double >( inbuf[ k1 ] ) * std::exp( std::complex< long double >( 0, static_cast< long double >( k1 ) * phinc ));
      }
      totalpower += std::norm( acc );
      difpower += std::norm( acc - std::complex< long double >( outbuf[ k0 ] ));
   }
   return static_cast< T >( std::sqrt( difpower / totalpower )); // Root mean square error
}

template< typename T >
T test_RDFTi( dip::uint nfft ) {
   // Initialize
   dip::RDFT< T > opts( nfft, true );
   dip::uint nIn = nfft / 2 + 1;
   // Create test data
   std::vector< std::complex< T >> inbuf( nIn + 1 );
   std::vector< T > outbuf( nfft );
   dip::Random random;
   inbuf.back() = 1e6; // Make sure we're not reading past where we're supposed to
   inbuf[ 0 ] = static_cast< T >( random() ) / static_cast< T >( random.max() ) - T( 0.5 );
   if(( nfft & 1u ) == 0 ) {
      --nIn;
      inbuf[ nIn ] = static_cast< T >( random() ) / static_cast< T >( random.max() ) - T( 0.5 );
   }
   for( dip::uint k = 1; k < nIn; ++k ) {
      inbuf[ k ] = std::complex< T >( static_cast< T >( random() ), static_cast< T >( random() )) / static_cast< T >( random.max() ) - T( 0.5 );
   }
   // Do the thing
   opts.Apply( reinterpret_cast< T* >( inbuf.data() ), outbuf.data(), T( 1 ));
   // Check
   long double totalpower = 0;
   long double difpower = 0;
   for( dip::uint k0 = 0; k0 < nfft; ++k0 ) {
      long double phinc = 2 * static_cast< long double >( k0 ) * pi / static_cast< long double >( nfft );
      //long double acc = inbuf[ 0 ].real();
      std::complex< long double > acc = inbuf[ 0 ];
      for( dip::uint k1 = 1; k1 < nIn; ++k1 ) {
         //acc += 2 * ( std::complex< long double >( inbuf[ k1 ] ) * std::exp( std::complex< long double >( 0, static_cast< long double >( k1 ) * phinc )) ).real();
         acc += std::complex< long double >( inbuf[ k1 ] ) * std::exp( std::complex< long double >( 0, static_cast< long double >( k1 ) * phinc ));
         acc += std::conj( std::complex< long double >( inbuf[ k1 ] )) * std::exp( std::complex< long double >( 0, -static_cast< long double >( k1 ) * phinc ));
      }
      if(( nfft & 1u ) == 0 ) {
         acc += ( std::complex< long double >( inbuf[ nIn ] ) * std::exp( std::complex< long double >( 0, static_cast< long double >( nIn ) * phinc )) ).real();
      }
      totalpower += std::norm( acc );
      difpower += std::norm( acc - static_cast< long double >( outbuf[ k0 ] ));
   }
   return static_cast< T >( std::sqrt( difpower / totalpower )); // Root mean square error
}

DOCTEST_TEST_CASE("[DIPlib] testing the RDFT class") {
   // Test forward (R2C) transform for a few different sizes that have all different radixes.
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::sfloat >( 32 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::dfloat >( 32 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::dfloat >( 256 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::sfloat >( 105 )) == 0 ); // 3*5*7
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::dfloat >( 154 )) == 0 ); // 2*7*11
   DOCTEST_CHECK( doctest::Approx( test_RDFT< dip::sfloat >( 97 )) == 0 ); // prime
   // Idem for inverse (C2R) transform
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::sfloat >( 32 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::dfloat >( 32 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::dfloat >( 256 )) == 0 );
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::sfloat >( 105 )) == 0 ); // 3*5*7
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::dfloat >( 154 )) == 0 ); // 2*7*11
   DOCTEST_CHECK( doctest::Approx( test_RDFTi< dip::sfloat >( 97 )) == 0 ); // prime
}

#endif // DIP_CONFIG_ENABLE_DOCTEST
