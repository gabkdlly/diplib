%WATERSHED   Watershed
%
% SYNOPSIS:
%  image_out = watershed(image_in,connectivity,max_depth,max_size,flags)
%
% PARAMETERS:
%  connectivity: defines which pixels are considered neighbours: up to
%     'connectivity' coordinates can differ by maximally 1. Thus:
%     * A connectivity of 1 indicates 4-connected neighbours in a 2D image
%       and 6-connected in 3D.
%     * A connectivity of 2 indicates 8-connected neighbourhood in 2D, and
%       18 in 3D.
%     * A connectivity of 3 indicates a 26-connected neighbourhood in 3D.
%     Connectivity can never be larger than the image dimensionality.
%  max_depth, max_size: determine merging of regions.
%     A region up to 'max_size' pixels and up to 'max_depth' grey-value
%     difference will be merged. Set 'max_size' to 0 to not include size
%     in the constraint. 'max_depth'-only merging is equivalent to applying
%     the H-minima transform before the watershed.
%  flags: a cell array of strings, choose from:
%     - 'high first': reverse the sorting order.
%     - 'correct': compute the correct watershed (slower), see note below.
%     - 'labels': output a labelled image rather than a binary image.
%
% DEFAULTS:
%  connectivity = 1
%  max_depth = 0 (only merging within plateaus)
%  max_size = 0 (any size)
%  flags = {}
%
% NOTE:
%  By default, this function uses a fast watershed algorithm that yields
%  poor results if there are plateaus in the image (regions with constant
%  grey-value). It is possible to break up plateaus by adding nose to the
%  image and setting 'max_depth' to some small value. The alternative
%  is to provide the 'correct' flag, which causes this function to use
%  a slower algorithm that always yields correct results.
%
% NOTE 2:
%  The fast algorithm also skips all edge pixels, marking them as watershed
%  pixels. If this is undesireable, extend the image by one pixel on all
%  sides. The alternative is to provide the 'correct' flag, which causes this
%  function to use a slower algorithm that does correctly address all edge
%  pixels.
%
% NOTE 3:
%  Pixels in IMAGE_IN with a value of +INF are not processed, and will be
%  marked as watershed pixels. Use this to mask out parts of the image you
%  don't need processed.
%
% SEE ALSO:
%  waterseed, maxima, minima
%
% DIPlib:
%  This function calls the DIPlib functions dip::Watershed.

% (c)2017, Cris Luengo.
% Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
% Based on original DIPimage code: (c)1999-2014, Delft University of Technology.
%
% Licensed under the Apache License, Version 2.0 (the "License");
% you may not use this file except in compliance with the License.
% You may obtain a copy of the License at
%
%    http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS,
% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
% See the License for the specific language governing permissions and
% limitations under the License.
