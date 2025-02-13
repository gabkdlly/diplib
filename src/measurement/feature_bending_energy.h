/*
 * (c)2016-2022, Cris Luengo.
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


namespace dip {
namespace Feature {


class FeatureBendingEnergy : public ChainCodeBased {
   public:
      FeatureBendingEnergy() : ChainCodeBased( { "BendingEnergy", "Bending energy of object perimeter (chain-code method, 2D)", false } ) {};

      virtual ValueInformationArray Initialize( Image const& label, Image const&, dip::uint ) override {
         ValueInformationArray out( 1 );
         PhysicalQuantity pq = label.PixelSize().UnitLength().Power( - 1 );
         scale_ = pq.magnitude;
         out[ 0 ].units = pq.units;
         out[ 0 ].name = "";
         return out;
      }

      virtual void Measure( ChainCode const& chainCode, Measurement::ValueIterator output ) override {
         *output = chainCode.BendingEnergy() * scale_;
      }

   private:
      dfloat scale_;
};


} // namespace feature
} // namespace dip
