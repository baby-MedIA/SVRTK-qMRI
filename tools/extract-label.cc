/*
 * SVRTK : SVR reconstruction based on MIRTK
 *
 * Copyright 2018-2021 King's College London
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

// MIRTK
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

using namespace std;
using namespace mirtk;
 
// =============================================================================
// Auxiliary functions
// =============================================================================

// -----------------------------------------------------------------------------
void usage()
{
    cout << "Usage: mirtk extract-label [input_label_image] [output_label_image] [start_label_number] [end_label_number]\n" << endl;
    cout << endl;
    cout << "Function for extracting specific label range from a multi-label image." << endl;
    cout << endl;
    cout << "\t" << endl;
    cout << "\t" << endl;
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


    RealImage input_stack, output_mask;
    

    char *tmp_fname = NULL;
    UniquePtr<BaseImage> tmp_image;
    
    UniquePtr<ImageReader> image_reader;
    InitializeIOLibrary();


    
    //read input name
    tmp_fname = argv[1];
    input_stack.Read(tmp_fname);
    argc--;
    argv++;

    //read output name
    output_name = argv[1];
    argc--;
    argv++;
    
    int num_l1 = atoi(argv[1]);
    
    argc--;
    argv++;
    
    int num_l2 = atoi(argv[1]);
    
    argc--;
    argv++;
    

    RealImage output_stack = input_stack;
    output_stack = 0;
 
    int sh = 1;

     for (int x = sh; x < input_stack.GetX()-sh; x++) {
        for (int y = sh; y < input_stack.GetY()-sh; y++) {
            for (int z = sh; z < input_stack.GetZ()-sh; z++) {

                if (input_stack(x,y,z) < (num_l2+1) && input_stack(x,y,z) > (num_l1-1)) {
                    output_stack(x,y,z) = 1;
                }
                else {
                    output_stack(x,y,z) = 0;
                }
            }
        }
    }

    
    output_stack.Write(output_name);

    
    return 0;
    
}


