/*
 * SVRTK : SVR reconstruction based on MIRTK
 *
 * Copyright 2018-2020 King's College London
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "mirtk/Common.h"
#include "mirtk/Options.h"

#include "mirtk/NumericsConfig.h"
#include "mirtk/IOConfig.h"
#include "mirtk/TransformationConfig.h"
#include "mirtk/RegistrationConfig.h"

#include "mirtk/GenericImage.h" 
#include "mirtk/GenericRegistrationFilter.h"

#include "mirtk/Transformation.h"
#include "mirtk/HomogeneousTransformation.h"
#include "mirtk/RigidTransformation.h"

#include "mirtk/ImageReader.h"
#include "mirtk/Dilation.h"

using namespace mirtk;
using namespace std;
 
// =============================================================================
// Auxiliary functions
// =============================================================================

// -----------------------------------------------------------------------------
void usage()
{
    cout << "..." << endl;
    exit(1);
}
// -----------------------------------------------------------------------------

// =============================================================================
// Main function
// =============================================================================

// -----------------------------------------------------------------------------

int main(int argc, char **argv)
{

    char buffer[256];
    RealImage stack;

    char *output_name = NULL;

    
    RealImage input_stack;
    

    char *tmp_fname = NULL;
    UniquePtr<BaseImage> tmp_image;
    
    UniquePtr<ImageReader> image_reader;
    InitializeIOLibrary();


    tmp_fname = argv[1];
    input_stack.Read(tmp_fname); 
    argc--;
    argv++;


    double dz = input_stack.GetZSize();
    double thickness;

    if (dz > 2.6)
        thickness = dz;
    else 
        thickness = 2.5;
 

    cout << thickness << endl;

    
    return 0;
}

