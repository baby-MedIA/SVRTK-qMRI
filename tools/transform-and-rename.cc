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
#include "mirtk/IOConfig.h"
#include "mirtk/GenericImage.h"
#include "mirtk/ImageReader.h"

#include "svrtk/Utility.h"

using namespace std;
using namespace mirtk;
using namespace svrtk;
using namespace svrtk::Utility;

// =============================================================================
// Auxiliary functions
// =============================================================================

// -----------------------------------------------------------------------------

void usage()
{
    cout << "Usage: mirtk transform-and-rename [original_reference_nii_file] [input_image] [additional_suffix_for_the_transformed_output] [output_folder] " << endl;
    cout << endl;
    cout << "Function for tranforming and renaming input files to a reference original space." << endl;
    cout << endl;
    cout << "\t" << endl;
    cout << "\t" << endl;
    
    exit(0);
}

// -----------------------------------------------------------------------------

// =============================================================================
// Main function
// =============================================================================

// -----------------------------------------------------------------------------




int main(int argc, char **argv)
{
    
    
    if (argc != 5)
        usage();

    
    const char *tmp_fname;
    const char *file_end_fname;
    const char *out_folder_fname;
    
    UniquePtr<BaseImage> tmp_image;
    UniquePtr<ImageReader> image_reader;
    InitializeIOLibrary();


    //-------------------------------------------------------------------
   
    RealImage main_stack, main_mask;

    tmp_fname = argv[1];
    image_reader.reset(ImageReader::TryNew(tmp_fname));
    tmp_image.reset(image_reader->Run());
    main_stack = *tmp_image;
    
    string org_name_string(tmp_fname);
    
    argc--;
    argv++;

    
    tmp_fname = argv[1];
    main_mask.Read(tmp_fname);
    
    RigidTransformation *rigidTransf_mask = new RigidTransformation;
    TransformMask(main_stack, main_mask, *rigidTransf_mask);
    
    argc--;
    argv++;
    
    
    file_end_fname = argv[1];
    
    string file_end_fname_string(file_end_fname);
    
    argc--;
    argv++;
    
    
    out_folder_fname = argv[1];
    
    string out_folder_fname_string(out_folder_fname);
    
    
    //-------------------------------------------------------------------
    
    
    std::size_t pos = org_name_string.find(".nii");
    std::string main_name = org_name_string.substr (0, pos);
    std::string end_name = org_name_string.substr (pos, org_name_string.length());
    
    std::size_t pos2 = main_name.find_last_of("/");
    std::string begin_name = main_name.substr (pos2+1, main_name.length());
    
    string new_name = out_folder_fname_string + "/" + begin_name + file_end_fname_string + end_name;
    char *c_new_name = &new_name[0];
    
    main_mask.Write(c_new_name);
    
    cout << c_new_name << endl;
    

    //-------------------------------------------------------------------

    
    return 0;
}



