/*
 * DIPlib 3.0
 * This file contains functionality for the MATLAB interface.
 *
 * (c)2017, Flagship Biosciences. Written by Cris Luengo.
 * Based on dip_matlab_interface.h: (c)2015-2017, Cris Luengo.
*/

#ifndef DIP_MMORPH_H
#define DIP_MMORPH_H

#include <map>
#include <utility>

#include "diplib.h"

#include <morph4cpp.h>

/// \file
/// \brief This file defines the `dip_mmorph` namespace, functionality to interface *MMorph* and *DIPlib*.

/// \brief The `dip_mmorph` namespace contains the interface between *MMorph* and *DIPlib*.
///
/// *MMorph* is the *[SDC Morphology Toolbox for C++](http://www.mmorph.com/)*.
///
/// This namespace defines the functions needed to convert between *MMorph* `::Image` objects and *DIPlib*
/// `dip::Image` objects. Note that *MMorph* is pretty old code that does not use namespaces.
///
/// We define an `ExternalInterface` so that output images from *DIPlib* can yield an *MMorph* image,
/// and we define a function that encapsulates an *MMorph* image in a *DIPlib* image.
namespace dip_mmorph {


// A deleter that doesn't delete.
void VoidStripHandler( void const* p ) {};

/// \brief Passing an *MMorph* image to *DIPlib*, keeping ownership of the data.
///
/// This function "converts" an `::Image` object to a `dip::Image` object.
/// The `dip::Image` object will point to the data in the `::Image`.
///
/// An empty `::Image` produces a non-forged `dip::Image`.
dip::Image MmToDip( ::Image const& mm ) {
   // Find image properties
   if( mm.isnull() ) {
      return {};
   }
   dip::DataType dt;
   switch( mm.typecode() ) {
      case MM_BYTE: // not supported by C++ MMorph interface
         dt = dip::DT_SINT8;
         break;
      case MM_UBYTE:
         if( mm.isbinary() ) {
            dt = dip::DT_BIN;
         } else {
            dt = dip::DT_UINT8;
         }
         break;
      case MM_SHORT: // not supported by C++ MMorph interface
         dt = dip::DT_SINT16;
         break;
      case MM_USHORT:
         dt = dip::DT_UINT16;
         break;
      case MM_INT:
         dt = dip::DT_SINT32;
         break;
      case MM_FLOAT: // not supported by C++ MMorph interface
         dt = dip::DT_SFLOAT;
         break;
      case MM_DOUBLE: // not supported by C++ MMorph interface
         dt = dip::DT_DFLOAT;
         break;
      default:
         DIP_THROW( "MMorph image with unknown type code" );
   }
   dip::UnsignedArray sizes = { static_cast< dip::uint >( mm.width()), static_cast< dip::uint >( mm.height()) };
   dip::Tensor tensor( static_cast< dip::uint >( mm.depth() ));
   // Define proper strides
   dip::IntegerArray strides = { 1, mm.width() };
   dip::sint tstride = mm.width() * mm.height();
   // Create Image object
   std::shared_ptr< void > p(static_cast< void* >( mm.raster() ), VoidStripHandler );
   dip::Image out( p, dt, sizes, strides, tensor, tstride, nullptr );
   return out;
}

/// \brief A unique pointer to an *MMorph* image. We cannot allocate these on the stack in the
/// interface. Careful with copying!
using ImagePtr = std::unique_ptr< ::Image >;

/// \brief This class is the dip::ExternalInterface for the *MMorph* interface.
///
/// Use the following code when declaring images to be used as the output to a function:
/// ```cpp
///     dip_mmorph::ExternalInterface mm;
///     dip::Image img_out0 = mm.NewImage();
///     dip::Image img_out1 = mm.NewImage();
/// ```
/// This configures the images `img_out0` and `img_out1` such that, when they are
/// forged later on, an `::Image` object will be created to hold the pixel data.
///
/// The `%ExternalInterface` object owns the `::Image` objects. You need to keep it
/// around as long as you use the image objects returned by its `NewImage` method,
/// otherwise the data segments will be freed and the `dip::Image` objects will point
/// to non-existing data segments.
///
/// To retrieve the `::Image` object inside such a `dip::Image`, use the
/// `dip_mmorph::ExternalInterface::DipToMM` method:
/// ```cpp
///     dip_mmorph::ImagePtr img0 = mm.DipToMM( img_out0 );
///     dip_mmorph::ImagePtr img1 = mm.DipToMM( img_out1 );
///     mmEro( *img0 ); // you need to dereference the pointer to the image...
/// ```
/// If you don't use the `DipToMM` method, the `::Image` that contains the pixel data
/// will be destroyed when the dip::Image object goes out of scope. The `DipToMM` method
/// changes ownership of the `::Image` object from the `%ExternalInterface` to the
/// `dip_mmorph::ImagePtr` object returned. In this case, the `dip::Image` object is still
/// valid, and shares the data segment with the extracted `::Image`. If the
/// `dip_mmorph::ImagePtr` is destroyed, the data segment will be freed and the `dip::Image`
/// object will point to a non-existing data segment.
///
/// Remember to not assign a result into the images created with `NewImage`,
/// as the pixel data will be copied in the assignment.
/// Instead, use the *DIPlib* functions that take output images as function
/// arguments:
/// ```cpp
///     img_out0 = in1 + in2;                                                                    // Bad!
///     dip::Add( in1, in2, out, DataType::SuggestArithmetic( in1.DataType(), in1.DataType() )); // Correct
/// ```
/// In the first case, `in1 + in2` is computed into a temporary image, whose
/// pixels are then copied into the `::Image` created for `img_out0`. In the
/// second case, the result of the operation is directly written into the
/// `mxArray`, no copies are necessary.
class ExternalInterface : public dip::ExternalInterface {
   private:
      // This map holds ::Images, we can find the right one if we have the data pointer.
      std::map< void const*, ImagePtr > images_;
      // This is the deleter functor we'll associate to the shared_ptr.
      class StripHandler {
         private:
            ExternalInterface& interface;
         public:
            StripHandler( ExternalInterface& mm ) : interface{ mm } {};
            void operator()( void const* p ) {
               interface.images_.erase( p );
            };
      };

   public:
      // This function overrides `dip::ExternalInterface::AllocateData`.
      // It is called when an image with this `ExternalInterface` is forged.
      // It allocates a *MMorph* `::Image` and returns a `std::shared_ptr` to the
      // data pointer, with a custom deleter functor. Strides are forced to
      // the only option available in *MMorph*, and an exception is thrown if
      // the data type or dimensionality is not supported.
      //
      // A user will never call this function directly.
      virtual std::shared_ptr< void > AllocateData(
            dip::UnsignedArray const& sizes,
            dip::IntegerArray& strides,
            dip::Tensor const& tensor,
            dip::sint& tstride,
            dip::DataType datatype
      ) override {
         DIP_THROW_IF( sizes.size() != 2, dip::E::DIMENSIONALITY_NOT_SUPPORTED );
         strides.resize( 2 );
         strides[ 0 ] = 1;
         strides[ 1 ] = sizes[ 0 ];
         tstride = sizes[ 0 ] * sizes[ 1 ]; // tensor dimension is last ('depth')
         char const* typestr;
         switch( datatype ) {
            case dip::DT_BIN:
               typestr = "binary";
               break;
            case dip::DT_UINT8:
               typestr = "uint8";
               break;
            case dip::DT_UINT16:
               typestr = "uint16";
               break;
            case dip::DT_SINT32:
               typestr = "int32";
               break;
            default:
               DIP_THROW( dip::E::DATA_TYPE_NOT_SUPPORTED );
         }
         // Create ::Image
         ImagePtr mm = (ImagePtr)new ::Image(static_cast< int >( sizes[ 0 ] ), static_cast< int >( sizes[ 1 ] ), static_cast< int >( tensor.Elements() ), typestr, 0.0 );
         void* ptr = mm->raster();
         images_.emplace( ptr, std::move( mm ));
         return std::shared_ptr< void >( ptr, StripHandler( *this ));
      }

      /// \brief Returns the MMorph `::Image` that holds the data for the `dip::Image` `img`.
      ///
      /// This function transfers ownership of the `::Image` object to the `dip_mmorph::ImagePtr`
      /// output object. Dereference to output object to access the *MMorph* image.
      ///
      /// The *MMorph* `::Image` returned is the one allocated to hold the pixel data in the input
      /// `img`. If `img` is a view of another image, the output will be the full image, not only
      /// the view. If `img` was obtained by indexing, or contains permuted and/or mirrored dimensions,
      /// then you need to make a copy first:
      /// ```cpp
      ///     dip_mmorph::ExternalInterface mm;
      ///     dip::Image img = mm.NewImage();
      ///     img.ReForge( { 256, 256 }, 1, dip::DT_UINT8 );
      ///     img = img.At( { 0, 128 }, { 0, 128 } ); // `img` is a view of the original image
      ///     img.Mirror( { true, false } );          // `img` has a mirrored dimension
      ///     // Make a copy, then extract the *MMorph* image:
      ///     dip::Image tmp = mm.NewImage();
      ///     tmp.Copy( img );                        // `tmp` now is a new image where the pixels have been copied from `img`
      ///     dip_mmorph::ImagePtr pImgMM = mm.DipToMM( tmp );
      /// ```
      ImagePtr DipToMM( dip::Image const& img ) {
         DIP_THROW_IF( !img.IsForged(), dip::E::IMAGE_NOT_FORGED );
         // TODO: We should check for strides exactly matching image dimensions, and make a copy if not a match.
         void const* ptr = img.Data();
         ImagePtr out = std::move( images_[ ptr ]);
         DIP_THROW_IF( !out, "The image was not present in the dip_mmorph::ExternalInterface" );
         images_.erase( ptr );
         // If the image is binary, we need to convert non-zero values to 255, as they are expected in MMorph.
         // DIPlib always writes 1 for true.
         if( out->isbinary() ) {
            long N = out->width() * out->height() * out->depth();
            uchar* mptr = (uchar*) out->raster();
            for( long ii = 0; ii < N; ++ii, ++mptr ) {
               if( *mptr ) {
                  *mptr = 255;
               }
            }
         }
         return out;
      }

      /// \brief Constructs a `dip::Image` object with the external interface set so that,
      /// when forged, a *MMorph* `::Image` will be allocated to hold the samples.
      dip::Image NewImage() {
         dip::Image out;
         out.SetExternalInterface( this );
         return out;
      }
};


} // namespace dip_mmorph

#endif // DIP_MMORPH_H
