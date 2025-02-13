/*
 * (c)2017-2021, Flagship Biosciences, Inc., written by Cris Luengo.
 * (c)2022, Cris Luengo.
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

#include "pydip.h"
#include "diplib/generic_iterators.h"

namespace {

dip::Image BufferToImage( py::buffer& buf, bool auto_tensor = true ) {
   py::buffer_info info = buf.request();
   //std::cout << "--Constructing dip::Image from Python buffer.\n";
   //std::cout << "   info.ptr = " << info.ptr << '\n';
   //std::cout << "   info.format = " << info.format << '\n';
   //std::cout << "   info.ndims = " << info.shape.size() << '\n';
   //std::cout << "   info.size = " << info.size << '\n';
   //std::cout << "   info.itemsize = " << info.itemsize << '\n';
   //std::cout << "   info.shape[0] = " << info.shape[0] << '\n';
   //std::cout << "   info.shape[1] = " << info.shape[1] << '\n';
   //std::cout << "   info.strides[0] = " << info.strides[0] << '\n';
   //std::cout << "   info.strides[1] = " << info.strides[1] << '\n';
   // Data type
   dip::DataType datatype;
   switch( info.format[ 0 ] ) {
      case '?':
         datatype = dip::DT_BIN;
         break;
      case 'B':
         datatype = dip::DT_UINT8;
         break;
      case 'H':
         datatype = dip::DT_UINT16;
         break;
      case 'I':
         datatype = dip::DT_UINT32;
         break;
      case 'L':
         datatype = dip::DataType( static_cast<unsigned long>( 0 )); // This size varies for 32-bit and 64-bit Linux, and Windows.
         break;
      case 'K': // The documentation mentions this one
      case 'Q': // The pybind11 sources use this one
         datatype = dip::DT_UINT64;
         break;
      case 'b':
         datatype = dip::DT_SINT8;
         break;
      case 'h':
         datatype = dip::DT_SINT16;
         break;
      case 'i':
         datatype = dip::DT_SINT32;
         break;
      case 'l':
         datatype = dip::DataType( long{} ); // This size varies for 32-bit and 64-bit Linux, and Windows.
         break;
      case 'k': // The documentation mentions this one
      case 'q': // The pybind11 sources use this one
         datatype = dip::DT_SINT64;
         break;
      case 'f':
         datatype = dip::DT_SFLOAT;
         break;
      case 'd':
         datatype = dip::DT_DFLOAT;
         break;
      case 'Z':
         switch( info.format[ 1 ] ) {
            case 'f':
               datatype = dip::DT_SCOMPLEX;
               break;
            case 'd':
               datatype = dip::DT_DCOMPLEX;
               break;
            default:
               std::cout << "Attempted to convert buffer to dip.Image object: data type not compatible.\n";
               DIP_THROW( "Buffer data type not compatible with class Image" );
         }
         break;
      default:
         std::cout << "Attempted to convert buffer to dip.Image object: data type not compatible.\n";
         DIP_THROW( "Buffer data type not compatible with class Image" );
   }
   // An empty array leads to a raw image
   dip::uint ndim = static_cast< dip::uint >( info.ndim );
   DIP_ASSERT( ndim == info.shape.size() );
   bool isEmpty = false;
   for( dip::uint ii = 0; ii < ndim; ++ii ) {
      isEmpty |= info.shape[ ii ] == 0;
   }
   //std::cout << "isEmpty = " << isEmpty << '\n';
   if( isEmpty ) {
      dip::Image out;
      out.SetDataType( datatype );
      return out;
   }
   // Sizes, optionally reversed
   dip::UnsignedArray sizes( ndim, 1 );
   for( dip::uint ii = 0; ii < ndim; ++ii ) {
      sizes[ ii ] = static_cast< dip::uint >( info.shape[ ii ] );
   }
   // Strides, also optionally reversed
   dip::IntegerArray strides( ndim, 1 );
   for( dip::uint ii = 0; ii < ndim; ++ii ) {
      dip::sint s = info.strides[ ii ] / static_cast< dip::sint >( info.itemsize );
      DIP_THROW_IF( s * static_cast< dip::sint >( info.itemsize ) != info.strides[ ii ],
                    "Cannot create image out of an array where strides are not in whole pixels" );
      strides[ ii ] = s;
   }
   // Optionally reverse dimensions
   if( ReverseDimensions() ) {
      sizes.reverse();
      strides.reverse();
   }
   // The containing Python object. We increase its reference count, and create a unique_ptr that decreases
   // its reference count again.
   PyObject* pyObject = buf.ptr();
   //std::cout << "   *** Incrementing ref count for pyObject " << pyObject << '\n';
   Py_XINCREF( pyObject );
   dip::DataSegment dataSegment{ pyObject, []( void* obj ) {
      //std::cout << "   *** Decrementing ref count for pyObject " << obj << '\n';
      PyGILState_STATE gstate;
      gstate = PyGILState_Ensure();
      Py_XDECREF( static_cast< PyObject* >( obj ));
      PyGILState_Release(gstate);
   }};
   // Create an image with all of this.
   dip::Image out( dataSegment, info.ptr, datatype, sizes, strides, {}, 1 );

   if( auto_tensor ) {
      // If it's a 3D image and the first or last dimension has <10 pixels, let's assume it's a tensor dimension:
      if( (sizes.size() > 2) && (sizes[0] < 10 || sizes[ndim - 1] < 10) ) {
         if( sizes[0] < sizes[ndim - 1] ) {
            out.SpatialToTensor( 0 );
         } else {
            out.SpatialToTensor( ndim - 1 );
         }
      }
   }
   return out;
}

dip::Image BufferToImage( py::buffer& buf, dip::sint tensor_axis ) {
   auto img = BufferToImage( buf, false );
   tensor_axis = -tensor_axis - 1;
   if( tensor_axis < 0 ) {
      tensor_axis += static_cast< dip::sint >( img.Dimensionality() );
   }
   img.SpatialToTensor( static_cast< dip::uint >( tensor_axis ));
   return img;
}

py::buffer_info ImageToBuffer( dip::Image const& image ) {
   // Get data type and size
   dip::String format;
   switch( image.DataType() ) {
      case dip::DT_BIN:
         format = "?";
         break;
      case dip::DT_UINT8:
         format = "B";
         break;
      case dip::DT_UINT16:
         format = "H";
         break;
      case dip::DT_UINT32:
         format = "I";
         break;
      case dip::DT_UINT64:
         format = "Q";
         break;
      case dip::DT_SINT8:
         format = "b";
         break;
      case dip::DT_SINT16:
         format = "h";
         break;
      case dip::DT_SINT32:
         format = "i";
         break;
      case dip::DT_SINT64:
         format = "q";
         break;
      case dip::DT_SFLOAT:
         format = "f";
         break;
      case dip::DT_DFLOAT:
         format = "d";
         break;
      case dip::DT_SCOMPLEX:
         format = "Zf";
         break;
      case dip::DT_DCOMPLEX:
         format = "Zd";
         break;
      default:
         DIP_THROW( "Image of unknown type" ); // should never happen
   }
   dip::sint itemsize = static_cast< dip::sint >( image.DataType().SizeOf() );
   // Short cut for non-forged images
   if( !image.IsForged() ) {
      //std::cout << "Getting buffer of non-forged image\n";
      return py::buffer_info{ nullptr, itemsize, format, 1, { 0 }, { 1 } };
   }
   // Get sizes and strides
   dip::UnsignedArray sizes = image.Sizes();
   dip::IntegerArray strides = image.Strides();
   for( dip::sint& s : strides ) {
      s *= itemsize;
   }
   // Optionally reverse sizes and strides arrays
   if( ReverseDimensions() ) {
      sizes.reverse();
      strides.reverse();
   }
   // Add tensor dimension as the last array dimension
   if( !image.IsScalar() ) {
      sizes.push_back( image.TensorElements() );
      strides.push_back( image.TensorStride() * itemsize );
   }
   py::buffer_info info{ image.Origin(), itemsize, format, static_cast< py::ssize_t >( sizes.size() ), sizes, strides };
   //std::cout << "--Constructed Python buffer for dip::Image object.\n";
   //std::cout << "   info.ptr = " << info.ptr << '\n';
   //std::cout << "   info.format = " << info.format << '\n';
   //std::cout << "   info.ndims = " << info.ndims << '\n';
   //std::cout << "   info.size = " << info.size << '\n';
   //std::cout << "   info.itemsize = " << info.itemsize << '\n';
   //std::cout << "   info.shape[0] = " << info.shape[0] << '\n';
   //std::cout << "   info.shape[1] = " << info.shape[1] << '\n';
   //std::cout << "   info.strides[0] = " << info.strides[0] << '\n';
   //std::cout << "   info.strides[1] = " << info.strides[1] << '\n';
   return info;
}

dip::String ImageRepr( dip::Image const& image ) {
   if( !image.IsForged() ) {
      return "<Empty image>";
   }
   std::ostringstream os;
   if( image.IsColor() ) {
      os << "<Color image (" << image.Tensor() << ", " << image.ColorSpace() << ")";
   } else if( !image.IsScalar() ) {
      os << "<Tensor image (" << image.Tensor() << ")";
   } else {
      os << "<Scalar image";
   }
   os << ", " << image.DataType();
   if( image.Dimensionality() == 0 ) {
      os << ", 0D";
   } else {
      os << ", sizes " << image.Sizes();
   }
   os << ">";
   return os.str();
}

} // namespace

void init_image( py::module& m ) {

   auto img = py::class_< dip::Image >( m, "Image", py::buffer_protocol(), "The class that encapsulates DIPlib images of all types." );
   // Constructor for raw (unforged) image, to be used e.g. when no mask input argument is needed:
   //   None implicitly converts to an image
   img.def( py::init<>() );
   img.def( py::init( []( py::none const& ) { return dip::Image{}; } ), "none"_a );
   py::implicitly_convertible< py::none, dip::Image >();
   // Constructor that takes a Sample: scalar implicitly converts to an image
   img.def( py::init< dip::Image::Sample const& >(), "sample"_a );
   img.def( py::init< dip::Image::Sample const&, dip::DataType >(), "sample"_a, "dt"_a );
   py::implicitly_convertible< dip::Image::Sample, dip::Image >();
   // Constructor that takes a Python raw buffer
   img.def( py::init( []( py::buffer& buf ) { return BufferToImage( buf ); } ), "array"_a);
   img.def( py::init( []( py::buffer& buf, py::none const& ) { return BufferToImage( buf, false ); } ), "array"_a, "none"_a );
   img.def( py::init( []( py::buffer& buf, dip::sint tensor_axis ) { return BufferToImage( buf, tensor_axis ); } ), "array"_a, "tensor_axis"_a );
   py::implicitly_convertible< py::buffer, dip::Image >();
   // Export a Python raw buffer
   img.def_buffer( []( dip::Image& self ) -> py::buffer_info { return ImageToBuffer( self ); } );
   // Constructor, generic
   img.def( py::init< dip::UnsignedArray const&, dip::uint, dip::DataType >(), "sizes"_a, "tensorElems"_a = 1, "dt"_a = dip::DT_SFLOAT );
   // Create new similar image
   img.def( "Similar", py::overload_cast< >( &dip::Image::Similar, py::const_ ));
   img.def( "Similar", py::overload_cast< dip::DataType >( &dip::Image::Similar, py::const_ ), "dt"_a );

   // Basic properties
   img.def( "__repr__", &ImageRepr );
   img.def( "__str__", []( dip::Image const& self ) { std::ostringstream os; os << self; return os.str(); } );
   img.def( "__len__", []( dip::Image const& self ) { return self.IsForged() ? self.NumberOfPixels() : 0; } );
   img.def( "IsForged", &dip::Image::IsForged, "See also `IsEmpty()`." );
   img.def( "IsEmpty", []( dip::Image const& self ) { return !self.IsForged(); },
            "Returns `True` if the image is raw. Reverse of `IsForged()`." ); // Honestly, I don't remember why I created this...
   img.def( "Dimensionality", &dip::Image::Dimensionality );
   img.def( "Sizes", &dip::Image::Sizes );
   img.def( "Size", &dip::Image::Size, "dim"_a );
   img.def( "NumberOfPixels", &dip::Image::NumberOfPixels );
   img.def( "NumberOfSamples", &dip::Image::NumberOfSamples );
   img.def( "Strides", &dip::Image::Strides );
   img.def( "Stride", &dip::Image::Stride, "dim"_a );
   img.def( "TensorStride", &dip::Image::TensorStride );
   img.def( "HasContiguousData", &dip::Image::HasContiguousData );
   img.def( "HasNormalStrides", &dip::Image::HasNormalStrides );
   img.def( "IsSingletonExpanded", &dip::Image::IsSingletonExpanded );
   img.def( "HasSimpleStride", &dip::Image::HasSimpleStride );
   img.def( "HasSameDimensionOrder", &dip::Image::HasSameDimensionOrder, "other"_a );
   img.def( "TensorSizes", &dip::Image::TensorSizes );
   img.def( "TensorElements", &dip::Image::TensorElements );
   img.def( "TensorColumns", &dip::Image::TensorColumns );
   img.def( "TensorRows", &dip::Image::TensorRows );
   img.def( "TensorShape", &dip::Image::TensorShape );
   img.def( "Tensor", &dip::Image::Tensor );
   img.def( "IsScalar", &dip::Image::IsScalar );
   img.def( "IsVector", &dip::Image::IsVector );
   img.def( "IsSquare", &dip::Image::IsSquare );
   img.def( "DataType", &dip::Image::DataType );
   img.def( "ColorSpace", &dip::Image::ColorSpace );
   img.def( "IsColor", &dip::Image::IsColor );
   img.def( "SetColorSpace", &dip::Image::SetColorSpace, "colorSpace"_a );
   img.def( "ResetColorSpace", &dip::Image::ResetColorSpace );
   img.def( "PixelSize", py::overload_cast<>( &dip::Image::PixelSize ));
   img.def( "PixelSize", py::overload_cast<>( &dip::Image::PixelSize, py::const_ ));
   img.def( "PixelSize", py::overload_cast< dip::uint >( &dip::Image::PixelSize, py::const_ ), "dim"_a );
   img.def( "SetPixelSize", py::overload_cast< dip::PixelSize >( &dip::Image::SetPixelSize ), "pixelSize"_a );
   img.def( "SetPixelSize", []( dip::Image& self, dip::dfloat mag, dip::Units const& units ) {
               self.SetPixelSize( dip::PhysicalQuantity( mag, units ));
            }, "magnitude"_a, "units"_a = dip::Units{},
            "Overload that accepts the two components of a `dip.PhysicalQuantity`, sets\n"
            "all dimensions to the same value." );
   img.def( "SetPixelSize", []( dip::Image& self, dip::FloatArray mags, dip::Units const& units ) {
               dip::PhysicalQuantityArray pq( mags.size() );
               for( dip::uint ii = 0; ii < mags.size(); ++ii ) {
                  pq[ ii ] = mags[ ii ] * units;
               }
               self.SetPixelSize( pq );
            }, "magnitudes"_a, "units"_a = dip::Units{},
            "Overload that accepts the two components of a `dip.PhysicalQuantity`, using\n"
            "a different magnitude for each dimension." );
   img.def( "SetPixelSize", py::overload_cast< dip::uint, dip::PhysicalQuantity >( &dip::Image::SetPixelSize ), "dim"_a, "sz"_a );
   img.def( "SetPixelSize", []( dip::Image& self, dip::uint dim, dip::dfloat mag, dip::Units const& units ) {
               self.SetPixelSize( dim, dip::PhysicalQuantity( mag, units ));
            }, "dim"_a, "magnitude"_a, "units"_a = dip::Units{},
            "Overload that accepts the two components of a `dip.PhysicalQuantity`, sets\n"
            "dimension `dim` only." );
   img.def( "ResetPixelSize", &dip::Image::ResetPixelSize );
   img.def( "HasPixelSize", &dip::Image::HasPixelSize );
   img.def( "IsIsotropic", &dip::Image::IsIsotropic );
   img.def( "PixelsToPhysical", &dip::Image::PixelsToPhysical, "array"_a );
   img.def( "PhysicalToPixels", &dip::Image::PhysicalToPixels, "array"_a );

   // About the data segment
   img.def( "IsShared", &dip::Image::IsShared );
   img.def( "ShareCount", &dip::Image::ShareCount );
   img.def( "SharesData", &dip::Image::SharesData, "other"_a );
   img.def( "Aliases", &dip::Image::Aliases, "other"_a );
   img.def( "IsIdenticalView", &dip::Image::IsIdenticalView, "other"_a );
   img.def( "IsOverlappingView", py::overload_cast< dip::Image const& >( &dip::Image::IsOverlappingView, py::const_ ), "other"_a );
   img.def( "Protect", &dip::Image::Protect, "set"_a = true );
   img.def( "IsProtected", &dip::Image::IsProtected );

   // Modify image without copying pixel data
   img.def( "PermuteDimensions", &dip::Image::PermuteDimensions, "order"_a, py::return_value_policy::reference_internal );
   img.def( "SwapDimensions", &dip::Image::SwapDimensions, "dim1"_a, "dim2"_a, py::return_value_policy::reference_internal );
   img.def( "ReverseDimensions", &dip::Image::ReverseDimensions, py::return_value_policy::reference_internal );
   img.def( "Flatten", &dip::Image::Flatten, py::return_value_policy::reference_internal );
   img.def( "FlattenAsMuchAsPossible", &dip::Image::FlattenAsMuchAsPossible, py::return_value_policy::reference_internal );
   img.def( "SplitDimension", &dip::Image::SplitDimension, "dim"_a, "size"_a, py::return_value_policy::reference_internal );
   img.def( "Squeeze", py::overload_cast<>( &dip::Image::Squeeze ), py::return_value_policy::reference_internal );
   img.def( "Squeeze", py::overload_cast< dip::uint >( &dip::Image::Squeeze ), "dims"_a, py::return_value_policy::reference_internal );
   img.def( "Squeeze", py::overload_cast< dip::UnsignedArray& >( &dip::Image::Squeeze ), "dim"_a, py::return_value_policy::reference_internal );
   img.def( "AddSingleton", py::overload_cast< dip::uint >( &dip::Image::AddSingleton ), "dim"_a, py::return_value_policy::reference_internal );
   img.def( "AddSingleton", py::overload_cast< dip::UnsignedArray const& >( &dip::Image::AddSingleton ), "dims"_a, py::return_value_policy::reference_internal );
   img.def( "ExpandDimensionality", &dip::Image::ExpandDimensionality, "dim"_a, py::return_value_policy::reference_internal );
   img.def( "ExpandSingletonDimension", &dip::Image::ExpandSingletonDimension, "dim"_a, "newSize"_a, py::return_value_policy::reference_internal );
   img.def( "ExpandSingletonDimensions", &dip::Image::ExpandSingletonDimensions, "newSizes"_a, py::return_value_policy::reference_internal );
   img.def( "UnexpandSingletonDimensions", &dip::Image::UnexpandSingletonDimensions, py::return_value_policy::reference_internal );
   img.def( "IsSingletonExpansionPossible", &dip::Image::IsSingletonExpansionPossible, "newSizes"_a );
   img.def( "ExpandSingletonTensor", &dip::Image::ExpandSingletonTensor, "size"_a, py::return_value_policy::reference_internal );
   img.def( "Mirror", py::overload_cast< dip::uint >( &dip::Image::Mirror ), "dimension"_a, py::return_value_policy::reference_internal );
   img.def( "Mirror", py::overload_cast< dip::BooleanArray >( &dip::Image::Mirror ), "process"_a, py::return_value_policy::reference_internal );
   img.def( "Rotation90", py::overload_cast< dip::sint, dip::uint, dip::uint >( &dip::Image::Rotation90 ), "n"_a, "dimension1"_a, "dimension2"_a, py::return_value_policy::reference_internal );
   img.def( "Rotation90", py::overload_cast< dip::sint, dip::uint >( &dip::Image::Rotation90 ), "n"_a, "axis"_a, py::return_value_policy::reference_internal );
   img.def( "Rotation90", py::overload_cast< dip::sint >( &dip::Image::Rotation90 ), "n"_a, py::return_value_policy::reference_internal );
   img.def( "StandardizeStrides", py::overload_cast<>( &dip::Image::StandardizeStrides ), py::return_value_policy::reference_internal );
   img.def( "ReshapeTensor", py::overload_cast< dip::uint, dip::uint >( &dip::Image::ReshapeTensor ), "rows"_a, "cols"_a, py::return_value_policy::reference_internal );
   img.def( "ReshapeTensor", py::overload_cast< dip::Tensor const& >( &dip::Image::ReshapeTensor ), "example"_a, py::return_value_policy::reference_internal );
   img.def( "ReshapeTensorAsVector", &dip::Image::ReshapeTensorAsVector, py::return_value_policy::reference_internal );
   img.def( "ReshapeTensorAsDiagonal", &dip::Image::ReshapeTensorAsDiagonal, py::return_value_policy::reference_internal );
   img.def( "Transpose", &dip::Image::Transpose, py::return_value_policy::reference_internal );
   img.def( "TensorToSpatial", py::overload_cast<>( &dip::Image::TensorToSpatial ), py::return_value_policy::reference_internal );
   img.def( "TensorToSpatial", py::overload_cast< dip::uint >( &dip::Image::TensorToSpatial ), "dim"_a, py::return_value_policy::reference_internal  );
   img.def( "SpatialToTensor", py::overload_cast<>( &dip::Image::SpatialToTensor ), py::return_value_policy::reference_internal );
   img.def( "SpatialToTensor", py::overload_cast< dip::uint >( &dip::Image::SpatialToTensor ), "dim"_a, py::return_value_policy::reference_internal );
   img.def( "SpatialToTensor", py::overload_cast< dip::uint, dip::uint >( &dip::Image::SpatialToTensor ), "rows"_a, "cols"_a, py::return_value_policy::reference_internal );
   img.def( "SpatialToTensor", py::overload_cast< dip::uint, dip::uint, dip::uint >( &dip::Image::SpatialToTensor ), "dim"_a, "rows"_a, "cols"_a, py::return_value_policy::reference_internal );
   img.def( "SplitComplex", py::overload_cast<>( &dip::Image::SplitComplex ), py::return_value_policy::reference_internal );
   img.def( "SplitComplex", py::overload_cast< dip::uint >( &dip::Image::SplitComplex ), "dim"_a, py::return_value_policy::reference_internal );
   img.def( "MergeComplex", py::overload_cast<>( &dip::Image::MergeComplex ), py::return_value_policy::reference_internal );
   img.def( "MergeComplex", py::overload_cast< dip::uint >( &dip::Image::MergeComplex ), "dim"_a, py::return_value_policy::reference_internal );
   img.def( "SplitComplexToTensor", &dip::Image::SplitComplexToTensor, py::return_value_policy::reference_internal );
   img.def( "MergeTensorToComplex", &dip::Image::MergeTensorToComplex, py::return_value_policy::reference_internal );
   img.def( "ReinterpretCast", &dip::Image::ReinterpretCast, py::return_value_policy::reference_internal );
   img.def( "ReinterpretCastToSignedInteger", &dip::Image::ReinterpretCastToSignedInteger, py::return_value_policy::reference_internal );
   img.def( "ReinterpretCastToUnsignedInteger", &dip::Image::ReinterpretCastToUnsignedInteger, py::return_value_policy::reference_internal );
   img.def( "Crop", py::overload_cast< dip::UnsignedArray const&, dip::String const& >( &dip::Image::Crop ),  "sizes"_a, "cropLocation"_a = "center", py::return_value_policy::reference_internal );

   // Create a view of another image.
   img.def( "Diagonal", []( dip::Image const& self ) -> dip::Image { return self.Diagonal(); } );
   img.def( "TensorRow", []( dip::Image const& self, dip::uint index ) -> dip::Image { return self.TensorRow( index ); }, "index"_a );
   img.def( "TensorColumn", []( dip::Image const& self, dip::uint index ) -> dip::Image { return self.TensorColumn( index ); }, "index"_a );
   img.def( "At", []( dip::Image const& self, dip::uint index ) -> dip::Image::Pixel { return self.At( index ); }, "index"_a );
   img.def( "At", []( dip::Image const& self, dip::uint x_index, dip::uint y_index ) -> dip::Image::Pixel { return self.At( x_index, y_index ); }, "x_index"_a, "y_index"_a );
   img.def( "At", []( dip::Image const& self, dip::uint x_index, dip::uint y_index, dip::uint z_index ) -> dip::Image::Pixel { return self.At( x_index, y_index, z_index ); }, "x_index"_a, "y_index"_a, "z_index"_a );
   img.def( "At", []( dip::Image const& self, dip::UnsignedArray const& coords ) -> dip::Image::Pixel { return self.At( coords ); }, "coords"_a );
   img.def( "At", []( dip::Image const& self, dip::Range x_range ) -> dip::Image { return self.At( x_range ); }, "x_range"_a );
   img.def( "At", []( dip::Image const& self, dip::Range x_range, dip::Range y_range ) -> dip::Image { return self.At( x_range, y_range ); }, "x_range"_a, "y_range"_a );
   img.def( "At", []( dip::Image const& self, dip::Range x_range, dip::Range y_range, dip::Range z_range ) -> dip::Image { return self.At( x_range, y_range, z_range ); }, "x_range"_a, "y_range"_a, "z_range"_a );
   img.def( "At", []( dip::Image const& self, dip::RangeArray ranges ) -> dip::Image { return self.At( std::move( ranges )); }, "ranges"_a );
   img.def( "At", []( dip::Image const& self, dip::Image mask ) -> dip::Image { return self.At( std::move( mask )); }, "mask"_a );
   img.def( "At", []( dip::Image const& self, dip::CoordinateArray const& coordinates ) -> dip::Image { return self.At( coordinates ); }, "coordinates"_a );
   img.def( "Cropped", []( dip::Image const& self, dip::UnsignedArray const& sizes, dip::String const& cropLocation ) -> dip::Image {
               return self.Cropped( sizes, cropLocation );
            },
            "sizes"_a, "cropLocation"_a = "center" );
   img.def( "Real", []( dip::Image const& self ) -> dip::Image { return self.Real(); } );
   img.def( "Imaginary", []( dip::Image const& self ) -> dip::Image { return self.Imaginary(); } );
   img.def( "QuickCopy", &dip::Image::QuickCopy );
   // These don't exist, but we need to have a function for operator[] too
   img.def( "__call__", []( dip::Image const& self, dip::sint index ) -> dip::Image { return self[ index ]; }, "index"_a );
   img.def( "__call__", []( dip::Image const& self, dip::uint i, dip::uint j ) -> dip::Image { return self[ dip::UnsignedArray{ i, j } ]; }, "i"_a, "j"_a );
   img.def( "__call__", []( dip::Image const& self, dip::Range const& range ) -> dip::Image { return self[ range ]; }, "range"_a );
   // NOTE: Compatibility with beta PyDIP. Remove?
   img.def( "TensorElement", []( dip::Image const& self, dip::sint index ) -> dip::Image { return self[ index ]; }, "index"_a );
   img.def( "TensorElement", []( dip::Image const& self, dip::uint i, dip::uint j ) -> dip::Image { return self[ dip::UnsignedArray{ i, j } ]; }, "i"_a, "j"_a );
   img.def( "TensorElement", []( dip::Image const& self, dip::Range const& range ) -> dip::Image { return self[ range ]; }, "range"_a );

   // Copy or write data
   img.def( "Pad", py::overload_cast< dip::UnsignedArray const&, dip::String const& >( &dip::Image::Pad, py::const_ ), "sizes"_a, "cropLocation"_a = "center" );
   img.def( "Copy", py::overload_cast<>( &dip::Image::Copy, py::const_ ));
   img.def( "Copy", py::overload_cast< dip::Image const& >( &dip::Image::Copy ), "src"_a );
   img.def( "Convert", &dip::Image::Convert, "dataType"_a );
   img.def( "SwapBytesInSample", &dip::Image::SwapBytesInSample );
   img.def( "ExpandTensor", &dip::Image::ExpandTensor );
   img.def( "Fill", py::overload_cast< dip::Image::Pixel const& >( &dip::Image::Fill ), "pixel"_a );
   img.def( "Mask", &dip::Image::Mask , "mask"_a );

   // Indexing into single pixel using coordinates
   img.def( "__getitem__", []( dip::Image const& self, dip::uint index ) -> dip::Image::Pixel { return self.At( index ); } );
   img.def( "__getitem__", []( dip::Image const& self, dip::UnsignedArray const& coords ) -> dip::Image::Pixel { return self.At( coords ); } );
   // Indexing into slice for 1D image
   img.def( "__getitem__", []( dip::Image const& self, dip::Range const& range ) -> dip::Image { return self.At( range ); } );
   // Indexing into slice for nD image
   img.def( "__getitem__", []( dip::Image const& self, dip::RangeArray rangeArray ) -> dip::Image { return self.At( std::move( rangeArray )); } );
   // Indexing using a mask image
   img.def( "__getitem__", []( dip::Image const& self, dip::Image mask ) -> dip::Image { return self.At( std::move( mask )); } );
   // Indexing using a list of coordinates
   img.def( "__getitem__", []( dip::Image const& self, dip::CoordinateArray const& coordinates ) -> dip::Image { return self.At( coordinates ); } );

    // Assignment into single pixel using linear index
   img.def( "__setitem__", []( dip::Image& self, dip::uint index, dip::Image::Sample const& v ) { self.At( index ) = v; } );
   img.def( "__setitem__", []( dip::Image& self, dip::uint index, dip::Image::Pixel const& v ) { self.At( index ) = v; } );
   // Assignment into single pixel using coordinates
   img.def( "__setitem__", []( dip::Image& self, dip::UnsignedArray const& coords, dip::Image::Sample const& v ) { self.At( coords ) = v; } );
   img.def( "__setitem__", []( dip::Image& self, dip::UnsignedArray const& coords, dip::Image::Pixel const& v ) { self.At( coords ) = v; } );
   // Assignment into slice for 1D image
   img.def( "__setitem__", []( dip::Image& self, dip::Range const& range, dip::Image::Pixel const& pixel ) { self.At( range ).Fill( pixel ); } );
   img.def( "__setitem__", []( dip::Image& self, dip::Range const& range, dip::Image const& source ) { self.At( range ).Copy( source ); } );
   // Assignment into slice for nD image
   img.def( "__setitem__", []( dip::Image& self, dip::RangeArray rangeArray, dip::Image::Pixel const& value ) { self.At( std::move( rangeArray )).Fill( value ); } );
   img.def( "__setitem__", []( dip::Image& self, dip::RangeArray rangeArray, dip::Image const& source ) { self.At( std::move( rangeArray )).Copy( source ); } );
   // Assignment using a mask image
   img.def( "__setitem__", []( dip::Image& self, dip::Image mask, dip::Image::Pixel const& pixel ) { self.At( std::move( mask )).Fill( pixel ); } );
   img.def( "__setitem__", []( dip::Image& self, dip::Image mask, dip::Image const& source ) { self.At( std::move( mask )).Copy( source ); } );
   // Assignment using a list of coordinates
   img.def( "__setitem__", []( dip::Image& self, dip::CoordinateArray const& coordinates, dip::Image::Pixel const& pixel ) { self.At( coordinates ).Fill( pixel ); } );
   img.def( "__setitem__", []( dip::Image& self, dip::CoordinateArray const& coordinates, dip::Image const& source ) { self.At( coordinates ).Copy( source ); } );

   // Operators
   img.def( py::self += py::self );
   img.def( py::self += dip::Image::Pixel() );
   img.def( py::self + py::self );
   img.def( py::self + dip::Image::Pixel() );
   img.def( dip::Image::Pixel() + py::self );
   img.def( py::self -= py::self );
   img.def( py::self -= dip::Image::Pixel() );
   img.def( py::self - py::self );
   img.def( py::self - dip::Image::Pixel() );
   img.def( dip::Image::Pixel() - py::self );
   img.def( "__imul__", []( dip::Image& self, dip::Image const& rhs ) { dip::MultiplySampleWise( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__imul__", []( dip::Image& self, dip::Image::Pixel const& rhs ) { dip::MultiplySampleWise( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__mul__", []( dip::Image const& lhs, dip::Image const& rhs ) { return dip::MultiplySampleWise( lhs, rhs ); }, py::is_operator() );
   img.def( "__mul__", []( dip::Image const& lhs, dip::Image::Pixel const& rhs ) { return dip::MultiplySampleWise( lhs, rhs ); }, py::is_operator() );
   img.def( "__rmul__", []( dip::Image const& rhs, dip::Image::Pixel const& lhs ) { return dip::MultiplySampleWise( dip::Image{ lhs }, rhs ); }, py::is_operator() );
   img.def( "__imatmul__", []( dip::Image& self, dip::Image const& rhs ) { dip::Multiply( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__imatmul__", []( dip::Image& self, dip::Image::Pixel const& rhs ) { dip::Multiply( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__matmul__", []( dip::Image const& lhs, dip::Image const& rhs ) { return dip::Multiply( lhs, rhs ); }, py::is_operator() );
   img.def( "__matmul__", []( dip::Image const& lhs, dip::Image::Pixel const& rhs ) { return dip::Multiply( lhs, rhs ); }, py::is_operator() );
   img.def( "__rmatmul__", []( dip::Image const& rhs, dip::Image::Pixel const& lhs ) { return dip::Multiply( dip::Image{ lhs }, rhs ); }, py::is_operator() );
   img.def( py::self /= py::self );
   img.def( py::self /= dip::Image::Pixel() );
   img.def( py::self / py::self );
   img.def( py::self / dip::Image::Pixel() );
   img.def( dip::Image::Pixel() / py::self );
   img.def( py::self %= py::self );
   img.def( py::self %= dip::Image::Pixel() );
   img.def( py::self % py::self );
   img.def( py::self % dip::Image::Pixel() );
   img.def( dip::Image::Pixel() % py::self );
   img.def( "__ipow__", []( dip::Image& self, dip::Image const& rhs ) { dip::Power( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__ipow__", []( dip::Image& self, dip::Image::Pixel const& rhs ) { dip::Power( self, rhs, self ); return self; }, py::is_operator() );
   img.def( "__pow__", []( dip::Image const& lhs, dip::Image const& rhs ) { return dip::Power( lhs, rhs ); }, py::is_operator() );
   img.def( "__pow__", []( dip::Image const& lhs, dip::Image::Pixel const& rhs ) { return dip::Power( lhs, rhs ); }, py::is_operator() );
   img.def( "__rpow__", []( dip::Image const& rhs, dip::Image::Pixel const& lhs ) { return dip::Power( dip::Image{ lhs }, rhs ); }, py::is_operator() );
   img.def( py::self == py::self );
   img.def( py::self == dip::Image::Pixel() );
   img.def( py::self != py::self );
   img.def( py::self != dip::Image::Pixel() );
   img.def( py::self > py::self );
   img.def( py::self > dip::Image::Pixel() );
   img.def( py::self >= py::self );
   img.def( py::self >= dip::Image::Pixel() );
   img.def( py::self < py::self );
   img.def( py::self < dip::Image::Pixel() );
   img.def( py::self <= py::self );
   img.def( py::self <= dip::Image::Pixel() );
   img.def( py::self & py::self );
   img.def( py::self & dip::Image::Pixel() );
   img.def( py::self | py::self );
   img.def( py::self | dip::Image::Pixel() );
   img.def( py::self ^ py::self );
   img.def( py::self ^ dip::Image::Pixel() );
   img.def( -py::self );
   img.def( "__invert__", []( dip::Image& self ) { return dip::Not( self ); }, py::is_operator() ); //img.def( ~py::self );

   // Some new functions useful in Python
   m.def( "Create0D", []( dip::Image::Pixel const& in ) -> dip::Image {
             return dip::Image( in );
          },
          "Function that creates a 0D image from a scalar or tensor value." );
   m.def( "Create0D", []( dip::Image const& in ) -> dip::Image {
             DIP_THROW_IF( !in.IsForged(), dip::E::IMAGE_NOT_FORGED );
             DIP_THROW_IF( !in.IsScalar(), dip::E::IMAGE_NOT_SCALAR );
             dip::UnsignedArray sz = in.Sizes();
             DIP_THROW_IF( sz.size() > 2, dip::E::DIMENSIONALITY_NOT_SUPPORTED );
             bool swapped = false;
             if( sz.size() == 2 ) {
                std::swap( sz[ 0 ], sz[ 1 ] ); // This way storage will be column-major
                swapped = true;
             } else {
                sz.resize( 2, 1 ); // add dimensions of size 1
             }
             dip::Image out( sz, 1, in.DataType() );
             if( swapped ) {
                out.SwapDimensions( 0, 1 ); // Swap dimensions so they match those of `in`
             }
             out.Copy( in ); // copy pixel data, don't re-use
             out.Flatten();
             out.SpatialToTensor( 0, sz[ 0 ], sz[ 1 ] );
             return out;
          },
          "Overload that takes a scalar image, the pixel values are the values for each\n"
          "tensor element of the output 0D image." );

   // Free functions in diplib/library/image.h
   m.def( "Copy", py::overload_cast< dip::Image const& >( &dip::Copy ), "src"_a );
   m.def( "ExpandTensor", py::overload_cast< dip::Image const& >( &dip::ExpandTensor ), "src"_a );
   m.def( "Convert", py::overload_cast< dip::Image const&, dip::DataType >( &dip::Convert ), "src"_a, "dt"_a );

}
