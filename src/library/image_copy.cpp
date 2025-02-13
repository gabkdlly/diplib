/*
 * (c)2014-2021, Cris Luengo.
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

#include <cstring> // std::memcpy

#include "diplib.h"
#include "diplib/statistics.h"
#include "diplib/framework.h"
#include "diplib/iterators.h"
#include "diplib/generic_iterators.h"
#include "diplib/overload.h"
#include "diplib/library/copy_buffer.h"

namespace dip {

namespace {

template< typename T >
void WriteSamples( dfloat const* src, void* destination, dip::uint n ) {
   T* dest = static_cast< T* >( destination );  // dest stride is always 1 (we created the pixel like that)
   dfloat const* end = src + n;                 // src stride is always 1 (it's an array)
   for( ; src != end; ++src, ++dest ) {
      *dest = clamp_cast< T >( *src );
   }
}

template< typename T >
void ReadSamples( void const* source, dfloat* dest, dip::uint n, dip::sint stride ) {
   T const* src = static_cast< T const* >( source );
   dfloat* end = dest + n;                            // dest stride is always 1 (it's an array)
   for( ; dest != end; src += stride, ++dest ) {
      *dest = clamp_cast< dfloat >( *src );
   }
}

} // namespace

Image::Pixel::Pixel( FloatArray const& values, dip::DataType dt ) : dataType_( dt ), tensor_( values.size() ) {
   SetInternalData();
   DIP_OVL_CALL_ALL( WriteSamples, ( values.data(), origin_, values.size() ), dataType_ );
}

Image::Pixel::operator FloatArray() const {
   FloatArray out( TensorElements() );
   DIP_OVL_CALL_ALL( ReadSamples, ( origin_, out.data(), out.size(), tensorStride_ ), dataType_ );
   return out;
}

//

void CopyFrom( Image const& src, Image& dest, Image const& srcMask ) {
   // Check input
   DIP_THROW_IF( !src.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( !srcMask.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_STACK_TRACE_THIS( srcMask.CheckIsMask( src.Sizes(), Option::AllowSingletonExpansion::DONT_ALLOW, Option::ThrowException::DO_THROW ));
   dip::uint N = Count( srcMask );
   if( !dest.IsForged() || ( dest.NumberOfPixels() != N ) || ( dest.TensorElements() != src.TensorElements() )) {
      DIP_STACK_TRACE_THIS( dest.ReForge( UnsignedArray( { N } ), src.TensorElements(), src.DataType(), Option::AcceptDataTypeChange::DO_ALLOW ));
      dest.CopyNonDataProperties( src );
   }
   if( dest.DataType() == src.DataType() ) {
      // Samples
      dip::uint telems = src.TensorElements();
      dip::uint bytes = dest.DataType().SizeOf(); // both source and destination have the same types
      if(( src.TensorStride() == 1 ) && ( dest.TensorStride() == 1 )) {
         // We copy the whole tensor as a single data block
         bytes *= telems;
         telems = 1;
      }
      // Iterate over *this and srcMask, copying pixels to destination
      GenericJointImageIterator< 2 > srcIt( { src, srcMask } );
      GenericImageIterator<> destIt( dest );
      if( telems == 1 ) { // most frequent case, really.
         do {
            if( *( static_cast< bin* >( srcIt.Pointer< 1 >() ))) {
               std::memcpy( destIt.Pointer(), srcIt.Pointer< 0 >(), bytes );
               ++destIt;
            }
         } while( ++srcIt );
      } else {
         do {
            if( *( static_cast< bin* >( srcIt.Pointer< 1 >() ))) {
               for( dip::uint ii = 0; ii < telems; ++ii ) {
                  std::memcpy( destIt.Pointer( ii ), srcIt.Pointer< 0 >( ii ), bytes );
               }
               ++destIt;
            }
         } while( ++srcIt );
      }
   } else {
      // Iterate over *this and srcMask, copying pixels to destination
      GenericJointImageIterator< 2 > srcIt( { src, srcMask } );
      GenericImageIterator<> destIt( dest );
      do {
         if( *( static_cast< bin* >( srcIt.Pointer< 1 >() ))) {
            DIP_ASSERT( destIt );
            *destIt = srcIt.Pixel< 0 >();
            ++destIt;
         }
      } while( ++srcIt );
   }
}

void CopyFrom( Image const& src, Image& dest, IntegerArray const& srcOffsets ) {
   // Check input
   DIP_THROW_IF( !src.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( srcOffsets.empty(), E::ARRAY_PARAMETER_EMPTY );
   if( !dest.IsForged() || ( dest.NumberOfPixels() != srcOffsets.size() ) || ( dest.TensorElements() != src.TensorElements() )) {
      DIP_STACK_TRACE_THIS( dest.ReForge( UnsignedArray( { srcOffsets.size() } ), src.TensorElements(), src.DataType(), Option::AcceptDataTypeChange::DO_ALLOW ));
      dest.CopyNonDataProperties( src );
   }
   if( dest.DataType() == src.DataType() ) {
      // Samples
      dip::uint telems = src.TensorElements();
      dip::uint bytes = dest.DataType().SizeOf(); // both source and destination have the same types
      if(( src.TensorStride() == 1 ) && ( dest.TensorStride() == 1 )) {
         // We copy the whole tensor as a single data block
         bytes *= telems;
         telems = 1;
      }
      // Iterate over srcOffsets and destination, copying pixels to destination
      auto arrIt = srcOffsets.begin();
      GenericImageIterator<> destIt( dest );
      if( telems == 1 ) { // most frequent case, really.
         do {
            std::memcpy( destIt.Pointer(), src.Pointer( *arrIt ), bytes );
         } while( ++arrIt, ++destIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.
      } else {
         do {
            dip::sint offset = *arrIt;
            for( dip::uint ii = 0; ii < telems; ++ii ) {
               std::memcpy( destIt.Pointer( ii ), src.Pointer( offset ), bytes );
               offset += src.TensorStride();
            }
         } while( ++arrIt, ++destIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.
      }
   } else {
      // Iterate over srcOffsets and destination, copying pixels to destination
      auto arrIt = srcOffsets.begin();
      GenericImageIterator<> destIt( dest );
      do {
         *destIt = Image::Pixel( src.Pointer( *arrIt ), src.DataType(), src.Tensor(), src.TensorStride() );
      } while( ++arrIt, ++destIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.

   }
}

void CopyTo( Image const& src, Image& dest, Image const& destMask ) {
   // Check input
   DIP_THROW_IF( !src.IsForged() || !dest.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( src.TensorElements() != dest.TensorElements(), E::NTENSORELEM_DONT_MATCH );
   DIP_THROW_IF( !destMask.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_STACK_TRACE_THIS( destMask.CheckIsMask( dest.Sizes(), Option::AllowSingletonExpansion::DONT_ALLOW, Option::ThrowException::DO_THROW ));
   if( dest.DataType() == src.DataType() ) {
      dip::uint telems = dest.TensorElements();
      dip::uint bytes = dest.DataType().SizeOf();
      if(( dest.TensorStride() == 1 ) && ( src.TensorStride() == 1 )) {
         // We copy the whole tensor as a single data block
         bytes *= telems;
         telems = 1;
      }
      // Iterate over dest and destMask, copying pixels from source
      GenericJointImageIterator< 2 > destIt( { dest, destMask } );
      GenericImageIterator<> srcIt( src );
      if( telems == 1 ) { // most frequent case, really.
         do {
            if( *( static_cast< bin* >( destIt.Pointer< 1 >() ))) {
               DIP_THROW_IF( !srcIt, E::SIZES_DONT_MATCH );
               std::memcpy( destIt.Pointer< 0 >(), srcIt.Pointer(), bytes );
               ++srcIt;
            }
         } while( ++destIt );
      } else {
         do {
            if( *( static_cast< bin* >( destIt.Pointer< 1 >() ))) {
               DIP_THROW_IF( !srcIt, E::SIZES_DONT_MATCH );
               for( dip::uint ii = 0; ii < telems; ++ii ) {
                  std::memcpy( destIt.Pointer< 0 >( ii ), srcIt.Pointer( ii ), bytes );
               }
               ++srcIt;
            }
         } while( ++destIt );
      }
      DIP_THROW_IF( srcIt, E::SIZES_DONT_MATCH );
   } else {
      // Iterate over dest and destMask, copying pixels from source
      GenericJointImageIterator< 2 > destIt( { dest, destMask } );
      GenericImageIterator<> srcIt( src );
      do {
         if( *( static_cast< bin* >( destIt.Pointer< 1 >() ))) {
            DIP_THROW_IF( !srcIt, E::SIZES_DONT_MATCH );
            destIt.Pixel< 0 >() = *srcIt;
            ++srcIt;
         }
      } while( ++destIt );
      DIP_THROW_IF( srcIt, E::SIZES_DONT_MATCH );
   }
}

void CopyTo( Image const& src, Image& dest, IntegerArray const& destOffsets ) {
   // Check input
   DIP_THROW_IF( !src.IsForged() || !dest.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( src.TensorElements() != dest.TensorElements(), E::NTENSORELEM_DONT_MATCH );
   DIP_THROW_IF( destOffsets.empty(), E::ARRAY_PARAMETER_EMPTY );
   DIP_THROW_IF( src.NumberOfPixels() != destOffsets.size(), "Number of pixels does not match offset list" );
   if( dest.DataType() == src.DataType() ) {
      dip::uint telems = dest.TensorElements();
      dip::uint bytes = dest.DataType().SizeOf(); // both source and destination have the same types
      if(( dest.TensorStride() == 1 ) && ( src.TensorStride() == 1 )) {
         // We copy the whole tensor as a single data block
         bytes *= telems;
         telems = 1;
      }
      // Iterate over destOffsets and src, copying pixels to dest
      auto arrIt = destOffsets.begin();
      GenericImageIterator<> srcIt( src );
      if( telems == 1 ) { // most frequent case, really.
         do {
            std::memcpy( dest.Pointer( *arrIt ), srcIt.Pointer(), bytes );
         } while( ++arrIt, ++srcIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.
      } else {
         do {
            dip::sint offset = *arrIt;
            for( dip::uint ii = 0; ii < telems; ++ii ) {
               std::memcpy( dest.Pointer( offset ), srcIt.Pointer( ii ), bytes );
               offset += dest.TensorStride();
            }
         } while( ++arrIt, ++srcIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.
      }
   } else {
      // Iterate over destOffsets and src, copying pixels to dest
      auto arrIt = destOffsets.begin();
      GenericImageIterator<> srcIt( src );
      do {
         Image::Pixel d( dest.Pointer( *arrIt ), dest.DataType(), dest.Tensor(), dest.TensorStride() );
         d = *srcIt;
      } while( ++arrIt, ++srcIt ); // these two must end at the same time, we test the image iterator, as arrIt should be compared with the end iterator.
   }
}

//

void Image::Copy( Image const& src ) {
   // TODO: allow copying with singleton expansion.
   DIP_THROW_IF( !src.IsForged(), E::IMAGE_NOT_FORGED );
   if( &src == this ) {
      // Copying self... what for?
      return;
   }
   if( IsForged() ) {
      if( IsIdenticalView( src )) {
         // Copy is a no-op, make sure additional properties are identical also
         CopyNonDataProperties( src );
         return;
      }
      if( !CompareProperties( src, Option::CmpProp::AllSizes, Option::ThrowException::DONT_THROW )
          || IsOverlappingView( src )) {
         // We cannot reuse the data segment
         DIP_STACK_TRACE_THIS( Strip() );
      } else {
         // We've got the data segment covered. Copy over additional properties
         CopyNonDataProperties( src );
      }
   }
   if( !IsForged() ) {
      auto* ei = externalInterface_;
      CopyProperties( src );
      externalInterface_ = ei; // This is only really relevant if `ei` is `nullptr`...
      DIP_STACK_TRACE_THIS( Forge() );
   }
   // A single CopyBuffer call if both images have simple strides and same dimension order
   dip::sint sstride_d;
   void* origin_d;
   std::tie( sstride_d, origin_d ) = GetSimpleStrideAndOrigin();
   if( origin_d ) {
      //std::cout << "dip::Image::Copy: destination has simple strides\n";
      dip::sint sstride_s;
      void* origin_s;
      std::tie( sstride_s, origin_s ) = src.GetSimpleStrideAndOrigin();
      if( origin_s ) {
         //std::cout << "dip::Image::Copy: source has simple strides\n";
         if( HasSameDimensionOrder( src )) {
            // No need to loop
            //std::cout << "dip::Image::Copy: no need to loop\n";
            detail::CopyBuffer(
                  origin_s,
                  src.dataType_,
                  sstride_s,
                  src.tensorStride_,
                  origin_d,
                  dataType_,
                  sstride_d,
                  tensorStride_,
                  NumberOfPixels(),
                  tensor_.Elements()
            );
            return;
         }
      }
   }
   // Otherwise, make nD loop
   //std::cout << "dip::Image::Copy: nD loop\n";
   dip::uint processingDim = Framework::OptimalProcessingDim( src );
   GenericJointImageIterator< 2 > it( { src, *this }, processingDim );
   dip::sint srcStride = src.strides_[ processingDim ];
   dip::sint destStride = strides_[ processingDim ];
   dip::uint nPixels = sizes_[ processingDim ];
   dip::uint nTElems = tensor_.Elements();
   do {
      detail::CopyBuffer(
            it.InPointer(),
            src.dataType_,
            srcStride,
            src.tensorStride_,
            it.OutPointer(),
            dataType_,
            destStride,
            tensorStride_,
            nPixels,
            nTElems
      );
   } while( ++it );
}

void Image::Copy( Image::View const& src ) {
   DIP_THROW_IF( TensorElements() != src.TensorElements(), E::NTENSORELEM_DONT_MATCH );
   if( src.IsRegular() ) {
      Copy( src.reference_ );
      return;
   }
   bool prot = IsProtected();
   if( IsForged() ) {
      Protect(); // prevent reforging. Instead, throw an error if image is not correct
   }
   if( src.mask_.IsForged() ) {
      DIP_STACK_TRACE_THIS( CopyFrom( src.reference_, *this, src.mask_ ));
   } else {
      DIP_ASSERT( !src.offsets_.empty() );
      DIP_STACK_TRACE_THIS( CopyFrom( src.reference_, *this, src.offsets_ ));
   }
   Protect( prot );
}

//

void ExpandTensor( Image const& c_in, Image& out ) {
   DIP_THROW_IF( !c_in.IsForged(), E::IMAGE_NOT_FORGED );

   if( c_in.Tensor().HasNormalOrder() ) {
      out = c_in;
      return;
   }

   //if(( &c_in == &out ) && ( TensorShape() == dip::Tensor::Shape::ROW_MAJOR_MATRIX )) {
   //   TODO: shuffle the data instead of copying it over to a new image.
   //   return
   //}

   // Separate input from output
   dip::Image in = c_in;

   // Prepare output image
   std::vector< dip::sint > lookUpTable = in.Tensor().LookUpTable();
   dip::Tensor tensor{ in.Tensor().Rows(), in.Tensor().Columns() };
   out.ReForge( in.Sizes(), tensor.Elements(), in.DataType(), Option::AcceptDataTypeChange::DO_ALLOW );
   out.ReshapeTensor( tensor );
   out.SetPixelSize( in.PixelSize() );
   // A single CopyBuffer call if both images have simple strides and same dimension order
   dip::sint outStride;
   void* outOrigin;
   std::tie( outStride, outOrigin ) = out.GetSimpleStrideAndOrigin();
   if( outOrigin ) {
      //std::cout << "dip::ExpandTensor: destination has simple strides\n";
      dip::sint inStride;
      void* inOrigin;
      std::tie( inStride, inOrigin ) = in.GetSimpleStrideAndOrigin();
      if( inOrigin ) {
         //std::cout << "dip::ExpandTensor: in has simple strides\n";
         if( out.HasSameDimensionOrder( in )) {
            // No need to loop
            //std::cout << "dip::ExpandTensor: no need to loop\n";
            detail::CopyBuffer(
                  inOrigin,
                  in.DataType(),
                  inStride,
                  in.TensorStride(),
                  outOrigin,
                  out.DataType(),
                  outStride,
                  out.TensorStride(),
                  out.NumberOfPixels(),
                  out.TensorElements(),
                  lookUpTable
            );
            return;
         }
      }
   }
   // Otherwise, make nD loop
   //std::cout << "dip::ExpandTensor: nD loop\n";
   dip::uint processingDim = Framework::OptimalProcessingDim( in );
   GenericJointImageIterator< 2 > it( { in, out }, processingDim );
   dip::sint inStride = in.Stride( processingDim );
   outStride = out.Stride( processingDim );
   dip::uint nPixels = out.Size( processingDim );
   do {
      detail::CopyBuffer(
            it.InPointer(),
            in.DataType(),
            inStride,
            in.TensorStride(),
            it.OutPointer(),
            out.DataType(),
            outStride,
            out.TensorStride(),
            nPixels,
            out.TensorElements(),
            lookUpTable
      );
   } while( ++it );
}

void Image::ExpandTensor() {
   if( !Tensor().HasNormalOrder() ) {
      DIP_STACK_TRACE_THIS( dip::ExpandTensor( *this, *this ));
   }
}

//

void Image::Convert( dip::DataType dt ) {
   DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
   if( dt != dataType_ ) {
      if(( dataType_ == DT_BIN ) && (( dt == DT_UINT8 ) || ( dt == DT_SINT8 ))) {
         // bin->uint8 or bin->sint8
         // These can happen without touching the data, it's OK even if the data is shared. Just change the flag.
         dataType_ = dt;
         return;
      }
      if( !IsShared() && ( dt.SizeOf() == dataType_.SizeOf() )) {
         // The operation can happen in place.
         // Loop over all pixels, casting with clamp each of the values; finally set the data type field.
         dip::sint sstride;
         void* origin;
         std::tie( sstride, origin ) = GetSimpleStrideAndOrigin();
         if( origin ) {
            // No need to loop
            //std::cout << "dip::Image::Convert: in-place, no need to loop\n";
            detail::CopyBuffer(
                  origin,
                  dataType_,
                  sstride,
                  tensorStride_,
                  origin,
                  dt,
                  sstride,
                  tensorStride_,
                  NumberOfPixels(),
                  tensor_.Elements()
            );
         } else {
            // Make nD loop
            //std::cout << "dip::Image::Convert: in-place, nD loop\n";
            dip::uint processingDim = Framework::OptimalProcessingDim( *this );
            GenericImageIterator<> it( *this, processingDim );
            it.OptimizeAndFlatten();
            do {
               detail::CopyBuffer(
                     it.Pointer(),
                     dataType_,
                     strides_[ processingDim ],
                     tensorStride_,
                     it.Pointer(),
                     dt,
                     strides_[ processingDim ],
                     tensorStride_,
                     sizes_[ processingDim ],
                     tensor_.Elements()
               );
            } while( ++it );
         }
         dataType_ = dt;
      } else {
         // We need to create a new data segment and copy it over.
         // Simply create a new image, identical copy of *this, with a different data type, copy
         // the data, then swap the two images.
         //std::cout << "dip::Image::Convert: using Copy\n";
         DIP_THROW_IF( IsProtected(), "Image is protected" );
         Image newimg;
         newimg.externalInterface_ = externalInterface_;
         newimg.ReForge( *this, dt );
         newimg.Copy( *this );
         this->move( std::move( newimg ));
      }
   }
}

//

namespace {

template< typename TPI >
void InternSwapBytesInSample( Image& img ) {
   ImageIterator< TPI > it( img );
   it.OptimizeAndFlatten();
   do {
      auto* c = reinterpret_cast< dip::uint8* >( it.Pointer() );
      for( dip::uint ii = 0; ii < sizeof( TPI ) / 2; ++ii ) {
         std::swap( c[ ii ], c[ sizeof( TPI ) - 1 - ii ] );
      }
   } while( ++it );
}

} // namespace

void Image::SwapBytesInSample() {
   DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
   if( DataType().SizeOf() == 1 ) {
      return; // nothing to do!
   }
   Image tmp = this->QuickCopy();
   if( !tmp.IsScalar() ) {
      tmp.TensorToSpatial();
   }
   if( tmp.DataType().IsComplex() ) {
      tmp.SplitComplex( 0 );
   }
   switch( DataType().SizeOf() ) {
      case 2:
         tmp.ReinterpretCast( DT_UINT16 );
         InternSwapBytesInSample< dip::uint16 >( tmp );
         break;
      case 4:
         tmp.ReinterpretCast( DT_UINT32 );
         InternSwapBytesInSample< dip::uint32 >( tmp );
         break;
      case 8:
         tmp.ReinterpretCast( DT_UINT64 );
         InternSwapBytesInSample< dip::uint64 >( tmp );
         break;
      default:
         DIP_THROW( E::NOT_IMPLEMENTED ); // This should never happen
   }
   // Not using DIP_OVL_CALL_UINT() here because it would instantiate the template for uint8 also, which we don't need and don't want.
}

//

namespace {

template< typename TPI >
static inline void InternFill( Image& dest, TPI value ) {
   DIP_THROW_IF( !dest.IsForged(), E::IMAGE_NOT_FORGED );
   dip::sint sstride;
   void* origin;
   std::tie( sstride, origin ) = dest.GetSimpleStrideAndOrigin();
   if( origin ) {
      // No need to loop
      detail::FillBufferFromTo( static_cast< TPI* >( origin ), sstride, dest.TensorStride(), dest.NumberOfPixels(), dest.TensorElements(), value );
   } else {
      // Make nD loop
      dip::uint processingDim = Framework::OptimalProcessingDim( dest );
      ImageIterator< TPI > it( dest, processingDim );
      it.OptimizeAndFlatten();
      dip::uint size = it.ProcessingDimensionSize();
      dip::sint stride = it.ProcessingDimensionStride();
      do {
         detail::FillBufferFromTo( it.Pointer(), stride, dest.TensorStride(), size, dest.TensorElements(), value );
      } while( ++it );
   }
}

} // namespace

void Image::Fill( Image::Pixel const& pixel ) {
   DIP_THROW_IF( !IsForged(), E::IMAGE_NOT_FORGED );
   dip::uint N = tensor_.Elements();
   if( pixel.TensorElements() == 1 ) {
      DIP_STACK_TRACE_THIS( Fill( pixel[ 0 ] ));
   } else {
      DIP_THROW_IF( pixel.TensorElements() != N, E::NTENSORELEM_DONT_MATCH );
      Image tmp = QuickCopy();
      tmp.tensor_.SetScalar();
      for( dip::uint ii = 0; ii < N; ++ii, tmp.origin_ = tmp.Pointer( tmp.tensorStride_ )) {
         // NOTE: tmp.Pointer( tmp.tensorStride_ ) takes the current tmp.origin_ and adds the tensor stride to it.
         // Thus, assigning this into tmp.origin_ is equivalent to tmp.origin += tmp_tensorStride_ if tmp.origin_
         // were a pointer to the correct data type.
         DIP_STACK_TRACE_THIS( tmp.Fill( pixel[ ii ] ));
      }
   }
}

void Image::Fill( Image::Sample const& sample ) {
   switch( dataType_ ) {
      case DT_BIN:      InternFill( *this, sample.As< bin      >() ); break;
      case DT_UINT8:    InternFill( *this, sample.As< uint8    >() ); break;
      case DT_SINT8:    InternFill( *this, sample.As< sint8    >() ); break;
      case DT_UINT16:   InternFill( *this, sample.As< uint16   >() ); break;
      case DT_SINT16:   InternFill( *this, sample.As< sint16   >() ); break;
      case DT_UINT32:   InternFill( *this, sample.As< uint32   >() ); break;
      case DT_SINT32:   InternFill( *this, sample.As< sint32   >() ); break;
      case DT_UINT64:   InternFill( *this, sample.As< uint64   >() ); break;
      case DT_SINT64:   InternFill( *this, sample.As< sint64   >() ); break;
      case DT_SFLOAT:   InternFill( *this, sample.As< sfloat   >() ); break;
      case DT_DFLOAT:   InternFill( *this, sample.As< dfloat   >() ); break;
      case DT_SCOMPLEX: InternFill( *this, sample.As< scomplex >() ); break;
      case DT_DCOMPLEX: InternFill( *this, sample.As< dcomplex >() ); break;
      default:
         DIP_THROW( E::DATA_TYPE_NOT_SUPPORTED );
   }
}

void Image::Mask( dip::Image const& mask ) {
   DIP_THROW_IF( !IsForged() || !mask.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_STACK_TRACE_THIS( mask.CheckIsMask( Sizes(), Option::AllowSingletonExpansion::DO_ALLOW, Option::ThrowException::DO_THROW ));
   DIP_STACK_TRACE_THIS( *this *= mask );
}

} // namespace dip


#ifdef DIP_CONFIG_ENABLE_DOCTEST
#include "doctest.h"

DOCTEST_TEST_CASE( "[DIPlib] testing dip::Image::SwapBytesInSample" ) {
   dip::Image img( { 5, 8 }, 3, dip::DT_SINT16 );
   img.Fill( 5 );
   DOCTEST_CHECK( img.At( 3, 5 )[ 1 ].As< dip::sint16 >() == 5 );
   img.SwapBytesInSample();
   DOCTEST_CHECK( img.At( 3, 5 )[ 1 ].As< dip::sint16 >() == 5 * 256 );
   img.SwapBytesInSample();
   DOCTEST_CHECK( img.At( 3, 5 )[ 1 ].As< dip::sint16 >() == 5 );

   img = dip::Image( { 5, 8 }, 1, dip::DT_SFLOAT );
   img.Rotation90( 1 );
   dip::sfloat v1 = 5.6904566e-28f; // hex representation: 12345678
   dip::sfloat v2 = 1.7378244e+34f; // hex representation: 78563412
   img.Fill( v1 );
   DOCTEST_CHECK( img.At( 4, 2 )[ 0 ].As< dip::sfloat >() == v1 );
   img.SwapBytesInSample();
   DOCTEST_CHECK( img.At( 4, 2 )[ 0 ].As< dip::sfloat >() == v2 );
   img.SwapBytesInSample();
   DOCTEST_CHECK( img.At( 4, 2 )[ 0 ].As< dip::sfloat >() == v1 );
}

#endif // DIP_CONFIG_ENABLE_DOCTEST
