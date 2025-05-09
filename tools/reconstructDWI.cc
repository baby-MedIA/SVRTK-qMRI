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

// SVRTK
#include "svrtk/ReconstructionDWI.h"

// C++ Standard
#include <iostream>
#include <chrono>
#include <ctime>
#include <fstream>
#include <cmath>
#include <set>
#include <algorithm>
#include <thread>
#include <functional>
#include <cstdlib>
#include <pthread.h>
#include <string>

using namespace std;
using namespace mirtk;
using namespace svrtk;

// =============================================================================
//
// =============================================================================

// -----------------------------------------------------------------------------

void usage()
{
    cerr << "Usage: reconstructDWI [reconstructed_signal] [N] [4D_DWI_stacks] [g_directions] [target] [init_transformation] <options>\n" << endl;
    cerr << endl;

    cerr << "\t[reconstructed_signal]   Name for the reconstructed signal volume." << endl;
    cerr << "\t[N]                      Number of 4D DWI stacks [Nifti]." << endl;
    cerr << "\t[4D_DWI_stack_1] .. [4D_DWI_stack_4]  The input DWI stacks. Nifti or Analyze format." << endl;
    cerr << "\t[bval_bgrad_file_1]  ..  [bval_bgrad_file_1]         List of .txt files with b-values and grad. directions for each DWI stack and direction in a text file." << endl;
    cerr << "\t[selected_b_value]       Selected b-value for reconstruction (>0)." << endl;
    cerr << "\t[T2_ref]                 Reference T2 volume in the atlas space." << endl;
    cerr << "\t[dof_to_T2_ref]          Tranformation [.dof] from the diffusion to the reference T2 volume." << endl;
    cerr << "\t" << endl;
    cerr << "Options:" << endl;
    cerr << "\t-dofin [dof_1]   .. [dof_N]    The transformations of the input stack to template" << endl;
    cerr << "\t                        in \'dof\' format used in MIRTK." <<endl;
    cerr << "\t                        Only rough alignment with correct orienation and " << endl;
    cerr << "\t                        some overlap is needed." << endl;
    cerr << "\t                        Use \'id\' for an identity transformation for at least" << endl;
    cerr << "\t                        one stack. The first stack with \'id\' transformation" << endl;
    cerr << "\t                        will be resampled as template." << endl;
    cerr << "\t-order [order]          SH order for reconstruction. [Default: 4]"<<endl;
    cerr << "\t-motion_model_hs        Option for procesing motion parameters. [Default: false]"<<endl;
    cerr << "\t-intensity_exclusion [low_slice_intensity] Set lowest intensity threshold for exclusion of dark slices. [Default: no intensity-based slice exclusion.]"<<endl;
    cerr << "\t-thickness [th]           Slice thickness for all stacks.[Default: voxel size in z direction]"<<endl;
    cerr << "\t-packages [n_packages]   Number of packages for all stacks.[Default: 1]"<<endl;
    cerr << "\t-mask [mask]              Binary mask to define the region of interest. [Default: whole image]"<<endl;
    cerr << "\t-iterations [iter]        Number of SVR-SR (registration-reconstruction) iterations. [Default: 9]"<<endl;
    cerr << "\t-sigma [sigma]            Stdev for bias field. [Default: 30mm]"<<endl;
    cerr << "\t-motion_sigma [motion_sigma] Sigma for SH reconstruction. [Default: 5mm]"<<endl;
    cerr << "\t-resolution [res]         Isotropic resolution of the output volumes. [Default: 1mm]"<<endl;
    cerr << "\t-multires [levels]        Multiresolution smooting with given number of levels. [Default: 3]"<<endl;
    cerr << "\t-delta [delta]            Parameter to define what is an edge. [Default: 0.001]"<<endl;
    cerr << "\t-lambda [lambda]          Smoothing parameter. [Default: 0.02]"<<endl;
    cerr << "\t-lambdaLB [lambdaLB]      Smoothing parameter. [Default: 0.0]"<<endl;
    cerr << "\t-regul_steps [steps]      Number of regularisation steps. [Default: 0]"<<endl;
    cerr << "\t-lastIter [lastIterLambda]  Smoothing parameter for last iteration. [Default: 0.01]"<<endl;
    cerr << "\t-smooth_mask [sigma]      Smooth the mask to reduce artefacts of manual segmentation. [Default: 0mm / none]"<<endl;
    cerr << "\t-global_bias_correction   Correct the bias in reconstructed image against previous estimation. [Default: false]"<<endl;
    cerr << "\t-low_intensity_cutoff [low_intensity]  Lower intensity threshold for inclusion of voxels in global bias correction. [Default: 0.01]"<<endl;
    cerr << "\t-transformations [folder] Use existing slice-to-volume transformations to initialize the reconstruction."<<endl;
    cerr << "\t-force_exclude [number of slices] [ind1] ... [indN]  Force exclusion of slices with these indices."<<endl;
    cerr << "\t-force_exclude_stacks [number of stacks] [ind1] ... [indN]  Force exclusion of whole stacks with these indices."<<endl;
    cerr << "\t-no_intensity_matching    Switch off intensity matching."<<endl;
    cerr << "\t-no_robust_statistics     Switch off robust statistics."<<endl;
    cerr << "\t-no_sh_intensity_matching   Switch off SH reconstruction intensity matching."<<endl;
    cerr << "\t-exclude_slices_only      Do not exclude individual voxels."<<endl;
    cerr << "\t-log_prefix [prefix]      Prefix for the log file."<<endl;
    cerr << "\t-info [filename]          Filename for slice information in\
    tab-sparated columns."<<endl;
    cerr << "\t-debug                    Debug mode - save intermediate results."<<endl;
    cerr << "\t-no_log                   Do not redirect cout and cerr to log files."<<endl;
    cerr << "\t" << endl;
    cerr << "\t" << endl;
    exit(1);
}




// -----------------------------------------------------------------------------

// =============================================================================
// Main function
// =============================================================================

// -----------------------------------------------------------------------------

int main(int argc, char **argv)
{


    UniquePtr<ImageReader> image_reader;
    InitializeIOLibrary();

    //utility variables
    int i, ok;
    char buffer[256];
    RealImage stack;

    //declare variables for input
    /// Name for output volume
    char * output_name = NULL;
    /// Slice stacks
    Array<RealImage> stacks;
    Array<RealImage> corrected_stacks;
    Array<string> stack_files;
    /// Stack transformation
    Array<RigidTransformation> stack_transformations;
    /// user defined transformations
    bool have_stack_transformations = false;
    /// Stack thickness
    Array<double > thickness;
    ///number of stacks
    int nStacks;
    /// number of packages for each stack
    Array<int> packages;


    RigidTransformation orient;

    // Default values.
    int templateNumber=-1;
    RealImage *mask=NULL;
    int iterations = 3;
    bool debug = false;
    double sigma=30;
    double resolution = 1;
    double lambda = 0.02;
    double delta = 0.001;
    int regul_steps = 0;
    int levels = 3;
    double lastIterLambda = 0.01;
    int rec_iterations;
    double averageValue = 700;
    double smooth_mask = 0;
    bool global_bias_correction = false;
    double low_intensity_cutoff = 0.01;
    //folder for slice-to-volume registrations, if given
    char * folder=NULL;
    //flag to remove black background, e.g. when neonatal motion correction is performed
    bool remove_black_background = false;
    //flag to swich the intensity matching on and off
    bool intensity_matching = true;
    bool intensity_matching_sh = true;
    bool rescale_stacks = false;

    //flag to swich the robust statistics on and off
    bool robust_statistics = true;
    bool robust_slices_only = false;
    //flag to replace super-resolution reconstruction by multilevel B-spline interpolation
    bool bspline = false;
    bool recon_1D = false;
    bool sh_only=false;

    int sr_sh_iterations = 10;
    double sh_alpha = 5;

    bool intensity_exclusion = false;
    double intensity_threshold = 4;


    bool motion_model_hs = false;

    double motion_sigma = 5;

    int order = 4;
    double lambdaLB = 0;//0.05;

    RealImage average;

    string info_filename = "slice_info.tsv";
    string log_id;
    bool no_log = false;

    //forced exclusion of slices
    int number_of_force_excluded_slices = 0;
    Array<int> force_excluded;

    //forced exclusion of stacks
    int number_of_force_excluded_stacks = 0;
    Array<int> force_excluded_stacks;

    //Create reconstruction object
    ReconstructionDWI reconstruction;



    //if not enough arguments print help
    if (argc < 3)
        usage();

    //read output name
    output_name = argv[1];
    argc--;
    argv++;
    cout<<"Recontructed DWI signal volume name ... "<<output_name<<endl;


    cout<<"Total number of input 4D DWI stacks ... "<<argv[1]<<endl;
    int global_nStacks = atoi(argv[1]);
    ok = true;
    argc--;
    argv++;


    nStacks = 0;

    Array<int> nStackDynamics;
    
    UniquePtr<BaseImage> tmp_image;

    // Read stacks
    for (int j=0; j<global_nStacks; j++)
    {
        //read 4D image
        RealImage image4D;
        cout<<"Reading stack " << j <<  " ... "<<argv[1]<<endl;
        
        image_reader.reset(ImageReader::TryNew(argv[1]));
        tmp_image.reset(image_reader->Run());
        image4D = *tmp_image;

//        image4D.Read(argv[1]);

        ImageAttributes attr = image4D.Attributes();
        int local_nStacks = attr._t;
        cout<<"Splitting into  ... "<<local_nStacks<<endl;

        nStackDynamics.push_back(local_nStacks);

        // Read stacks
        for (i=0; i<local_nStacks; i++)
        {

            stacks.push_back(image4D.GetRegion(0,0,0,i,attr._x, attr._y,attr._z,i+1));
            corrected_stacks.push_back(stacks[i]);

        }

        nStacks = nStacks + local_nStacks;

        argc--;
        argv++;

    }


    cout << "Total number of 3D stacks (diffusion weighted directions): " << nStacks << endl;


    Array<Array<double> > directions(3,Array<double>(nStacks+1,0));
    Array<double> bvalues(nStacks+1,0);

    int coord = 0;
    int dir = 0;
    double num;


    for (int j=0; j<global_nStacks; j++)
    {

        //Read the name of the textfile with directions
        char *textfile = argv[1];
        argc--;
        argv++;
        cout<<"Reading directions textfile " << j << " ... " <<textfile<<endl;

        int local_nStacks = nStackDynamics[j];

        ifstream in(textfile);

        coord = 0;
        int local_dir = 0;
        num = 0;

        cout<<"Reading directions: "<<endl;
        if (in.is_open())
        {

            while (!in.eof())
            {
                in >> num;


                if ((coord<4)&&(local_dir<local_nStacks))
                {
                    if(coord<3) {
                        cout << " (" << coord << "," << dir+1 << ") ";
                        directions[coord][dir+1]=num;
                    }
                    else
                        bvalues[dir+1]=num;
                }
                cout << num << " ";
                coord++;
                if (coord>=4)
                {
                    coord=0;
                    dir++;
                    local_dir++;
                    cout<<endl;
                }
            }
            in.close();
        }
        else
        {
            cout << "Unable to open file " << textfile << endl;
            exit(1);
        }


    }


    cout << endl;

    cout<<"Selected b-value for reconstructions ... "<<argv[1]<<endl;
    int main_bvalue = atoi(argv[1]);
    ok = true;
    argc--;
    argv++;

    cout << endl;

    int all_new_count = 0;

    for (i=0; i<nStacks; i++)
    {

        int dir_i = i+1;
        if ((bvalues[dir_i] < main_bvalue*0.95) || (bvalues[dir_i] > main_bvalue*1.05)) {
//            cout << i << " : " << directions[0][dir_i] << " " << directions[1][dir_i] << " " << directions[2][dir_i] << " " << bvalues[dir_i] << endl;
        } else {
            all_new_count = all_new_count + 1;
        }


    }

    cout << endl;

    int new_nStacks = 0;
    Array<RealImage> new_stacks;
    Array<Array<double> > new_directions(3,Array<double>(all_new_count+1,0));
    Array<double> new_bvalues(all_new_count+1,0);



    int total_excluded = 0;

    int new_dir_i = 1;


    ofstream out_b_file;
    sprintf(buffer, "final-b-file.b");
    out_b_file.open(buffer);


    cout<<"Excluding stacks with other b-values ... ";
    for (i=0; i<nStacks; i++)
    {
        int dir_i = i+1;
        if ((bvalues[dir_i] < main_bvalue*0.95) || (bvalues[dir_i] > main_bvalue*1.05)) {
            cout << i << "(" << bvalues[dir_i] << ") ";
            total_excluded = total_excluded + 1;
        } else {
            new_stacks.push_back(stacks[i]);


            out_b_file << directions[0][dir_i] << " " << directions[1][dir_i] << " " << directions[2][dir_i] << " " << bvalues[dir_i] << endl;

            new_directions[0][new_dir_i] = directions[0][dir_i];
            new_directions[1][new_dir_i] = directions[1][dir_i];
            new_directions[2][new_dir_i] = directions[2][dir_i];

            new_bvalues[new_dir_i] = bvalues[dir_i];

            new_nStacks = new_nStacks + 1;
            new_dir_i = new_dir_i + 1;
        }

    }


    out_b_file.close();

//    if (total_excluded > 0) {

        stacks.clear();
        bvalues.clear();
        directions.clear();
        corrected_stacks.clear();

        for (i=0; i<new_stacks.size(); i++) {
            int dir_i = i+1;
            stacks.push_back(new_stacks[i]);
            corrected_stacks.push_back(new_stacks[i]);

            sprintf(buffer,"stack%i.nii.gz",(i));
            new_stacks[i].Write(buffer);

//            cout << i << " : " << new_directions[0][dir_i] << " " << new_directions[1][dir_i] << " " << new_directions[2][dir_i] << " " << new_bvalues[dir_i] << endl;
        }

        nStacks = stacks.size();

//    }



    cout << endl ;
    cout << "Final number of stacks: " << nStacks << endl;
    cout << "Final .b file: final-b-file.b" << endl;


    //read target
    RealImage target(argv[1]);
    cout<<"Target is "<<argv[1]<<endl;
    argc--;
    argv++;

    //read initial orientation

    Transformation *t_org = Transformation::New(argv[1]);
    unique_ptr<RigidTransformation> rigidTransf_org(dynamic_cast<RigidTransformation*>(t_org));
    orient = *rigidTransf_org;
//    orient.Read(argv[1]);
    cout<<"Initial transformation is "<<argv[1]<<endl;
    argc--;
    argv++;
    orient.Print();




    // Parse options.
    while (argc > 1){
        ok = false;

        //Read stack transformations
        if ((ok == false) && (strcmp(argv[1], "-dofin") == 0)){
            argc--;
            argv++;

            for (i=0;i<nStacks;i++)
            {

                cout<<"Reading transformation ... "<<argv[1]<<" ... ";
                cout.flush();

                Transformation *t = Transformation::New(argv[1]);
                RigidTransformation *rigidTransf = dynamic_cast<RigidTransformation*> (t);
                stack_transformations.push_back(*rigidTransf);

                delete rigidTransf;

                cout<<" done."<<endl;

                argc--;
                argv++;

            }
            reconstruction.InvertStackTransformations(stack_transformations);
            have_stack_transformations = true;
        }

        //Read slice thickness
        if ((ok == false) && (strcmp(argv[1], "-thickness") == 0)){
            argc--;
            argv++;
            cout<< "Slice thickness is ";
            double thick = atof(argv[1]);
            argc--;
            argv++;
            for (i=0;i<nStacks;i++)
            {
                thickness.push_back(thick);
                cout<<thickness[i]<<" ";
            }
            cout<<"."<<endl;
            ok = true;
        }

        //Read number of packages for each stack
        if ((ok == false) && (strcmp(argv[1], "-packages") == 0)){
            argc--;
            argv++;
            int pnum = atoi(argv[1]);
            argc--;
            argv++;
            cout<< "Package number is "<<pnum<<endl;
            for (i=0;i<nStacks;i++)
            {
                packages.push_back(pnum);
                cout<<packages[i]<<" ";
            }
//            cout<<"size="<<packages.size()<<endl;
            cout << " . " <<  endl;
            ok = true;
        }

        //Read binary mask for final volume
        if ((ok == false) && (strcmp(argv[1], "-mask") == 0)){
            argc--;
            argv++;
            mask= new RealImage(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Read number of registration-reconstruction iterations
        if ((ok == false) && (strcmp(argv[1], "-iterations") == 0)){
            argc--;
            argv++;
            iterations=atoi(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Read number of registration-reconstruction iterations
        if ((ok == false) && (strcmp(argv[1], "-template") == 0)){
            argc--;
            argv++;
            templateNumber=atoi(argv[1]);
            ok = true;
            argc--;
            argv++;
        }



        //Read number of registration-reconstruction iterations
        if ((ok == false) && (strcmp(argv[1], "-sr_sh_iterations") == 0)){
            argc--;
            argv++;
            sr_sh_iterations=atoi(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Step for SR SH
        if ((ok == false) && (strcmp(argv[1], "-sh_alpha") == 0)){
            argc--;
            argv++;
            sh_alpha=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Variance of Gaussian kernel to smooth the bias field.
        if ((ok == false) && (strcmp(argv[1], "-sigma") == 0)){
            argc--;
            argv++;
            sigma=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Variance of Gaussian kernel to smooth the bias field.
        if ((ok == false) && (strcmp(argv[1], "-motion_sigma") == 0)){
            argc--;
            argv++;
            motion_sigma=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        if ((ok == false) && (strcmp(argv[1], "-intensity_exclusion") == 0)){

            intensity_exclusion = true;
            argc--;
            argv++;
            intensity_threshold=atof(argv[1]);

            cout << "Slice intensity exclusion threshold = " << intensity_threshold << endl;

            ok = true;
            argc--;
            argv++;

        }

        //Smoothing parameter
        if ((ok == false) && (strcmp(argv[1], "-lambda") == 0)){
            argc--;
            argv++;
            lambda=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Smoothing parameter
        if ((ok == false) && (strcmp(argv[1], "-lambdaLB") == 0)){
            argc--;
            argv++;
            lambdaLB=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Smoothing parameter for last iteration
        if ((ok == false) && (strcmp(argv[1], "-lastIter") == 0)){
            argc--;
            argv++;
            lastIterLambda=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Parameter to define what is an edge
        if ((ok == false) && (strcmp(argv[1], "-delta") == 0)){
            argc--;
            argv++;
            delta=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Parameter to define what is an edge
        if ((ok == false) && (strcmp(argv[1], "-regul_steps") == 0)){
            argc--;
            argv++;
            regul_steps=atoi(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Isotropic resolution for the reconstructed volume
        if ((ok == false) && (strcmp(argv[1], "-resolution") == 0)){
            argc--;
            argv++;
            resolution=atof(argv[1]);
            ok = true;
            argc--;
            argv++;
        }

        //Number of resolution levels
        if ((ok == false) && (strcmp(argv[1], "-multires") == 0)){
            argc--;
            argv++;
            levels=atoi(argv[1]);
            argc--;
            argv++;
            ok = true;
        }

        //Smooth mask to remove effects of manual segmentation
        if ((ok == false) && (strcmp(argv[1], "-smooth_mask") == 0)){
            argc--;
            argv++;
            smooth_mask=atof(argv[1]);
            argc--;
            argv++;
            ok = true;
        }

        //Switch off intensity matching
        if ((ok == false) && (strcmp(argv[1], "-motion_model_hs") == 0)){
            argc--;
            argv++;
            motion_model_hs=true;
            ok = true;
        }

        //Switch off intensity matching
        if ((ok == false) && (strcmp(argv[1], "-no_intensity_matching") == 0)){
            argc--;
            argv++;
            intensity_matching=false;
            intensity_matching_sh=false;
            ok = true;
        }

        //Switch off intensity matching
        if ((ok == false) && (strcmp(argv[1], "-no_vol_intensity_matching") == 0)){
            argc--;
            argv++;
            intensity_matching=false;
            ok = true;
        }

        //Switch off intensity matching
        if ((ok == false) && (strcmp(argv[1], "-no_sh_intensity_matching") == 0)){
            argc--;
            argv++;
            intensity_matching_sh=false;
            ok = true;
        }

        //Switch off robust statistics
        if ((ok == false) && (strcmp(argv[1], "-no_robust_statistics") == 0)){
            argc--;
            argv++;
            robust_statistics=false;
            ok = true;
        }

        //Switch off robust statistics
        if ((ok == false) && (strcmp(argv[1], "-exclude_slices_only") == 0)){
            argc--;
            argv++;
            robust_slices_only=true;
            ok = true;
        }

//        //Use multilevel B-spline interpolation instead of super-resolution
//        if ((ok == false) && (strcmp(argv[1], "-bspline") == 0)){
//            argc--;
//            argv++;
//            bspline=true;
//            ok = true;
//        }

        //Perform bias correction of the reconstructed image agains the GW image in the same motion correction iteration
        if ((ok == false) && (strcmp(argv[1], "-global_bias_correction") == 0)){
            argc--;
            argv++;
            global_bias_correction=true;
            ok = true;
        }

        if ((ok == false) && (strcmp(argv[1], "-low_intensity_cutoff") == 0)){
            argc--;
            argv++;
            low_intensity_cutoff=atof(argv[1]);
            argc--;
            argv++;
            ok = true;
        }

        //Debug mode
        if ((ok == false) && (strcmp(argv[1], "-debug") == 0)){
            argc--;
            argv++;
            debug=true;
            ok = true;
        }

        //Prefix for log files
        if ((ok == false) && (strcmp(argv[1], "-log_prefix") == 0)){
            argc--;
            argv++;
            log_id=argv[1];
            ok = true;
            argc--;
            argv++;
        }

        //No log files
        if ((ok == false) && (strcmp(argv[1], "-no_log") == 0)){
            argc--;
            argv++;
            no_log=true;
            ok = true;
        }

        // rescale stacks to avoid error:
        // irtkImageRigidRegistrationWithPadding::Initialize: Dynamic range of source is too large
        if ((ok == false) && (strcmp(argv[1], "-rescale_stacks") == 0)){
            argc--;
            argv++;
            rescale_stacks=true;
            ok = true;
        }

        // Save slice info
        if ((ok == false) && (strcmp(argv[1], "-info") == 0)) {
            argc--;
            argv++;
            info_filename=argv[1];
            ok = true;
            argc--;
            argv++;
        }

        //Read transformations from this folder
        if ((ok == false) && (strcmp(argv[1], "-transformations") == 0)){
            argc--;
            argv++;
            folder=argv[1];
            ok = true;
            argc--;
            argv++;
        }


        //Force removal of certain slices
        if ((ok == false) && (strcmp(argv[1], "-force_exclude") == 0)){
            argc--;
            argv++;
            number_of_force_excluded_slices = atoi(argv[1]);
            argc--;
            argv++;

            cout<< number_of_force_excluded_slices<< " force excluded slices: ";
            for (i=0;i<number_of_force_excluded_slices;i++)
            {
                force_excluded.push_back(atoi(argv[1]));
                cout<<force_excluded[i]<<" ";
                argc--;
                argv++;
            }
            cout<<"."<<endl;

            ok = true;
        }


        //Force removal of certain stacks
        if ((ok == false) && (strcmp(argv[1], "-force_exclude_stacks") == 0)){
            argc--;
            argv++;
            number_of_force_excluded_stacks = atoi(argv[1]);
            argc--;
            argv++;

            cout<< number_of_force_excluded_stacks<< " force excluded stacks: ";
            for (i=0;i<number_of_force_excluded_stacks;i++)
            {
                force_excluded_stacks.push_back(atoi(argv[1]));
                cout<<force_excluded_stacks[i]<<" ";
                argc--;
                argv++;
            }
            cout<<"."<<endl;

            ok = true;
        }

        //Perform reconstruction only in z direction
        if ((ok == false) && (strcmp(argv[1], "-1D") == 0)){
            argc--;
            argv++;
            recon_1D = true;
            ok = true;
        }

        //SH order
        if ((ok == false) && (strcmp(argv[1], "-order") == 0)){
            argc--;
            argv++;
            order = atoi(argv[1]);
            argc--;
            argv++;
            ok = true;
        }




        if (ok == false){
            cerr << "Can not parse argument " << argv[1] << endl;
            usage();
        }
    }

    if (rescale_stacks)
    {
        for (i=0;i<nStacks;i++)
            reconstruction.Rescale(stacks[i],1000);
    }

    if(!have_stack_transformations)
    {
        stack_transformations.clear();
        for (i=0;i<nStacks;i++)
        {
            stack_transformations.push_back(orient);
        }
        templateNumber = 0;
        reconstruction.InvertStackTransformations(stack_transformations);
    }

    //Initialise slice thickness if not given by user
    if (thickness.size()==0)
    {
        cout<< "Slice thickness is ";
        for (i=0;i<nStacks;i++)
        {
            double dx,dy,dz;
            stacks[i].GetPixelSize(&dx,&dy,&dz);
            thickness.push_back(dz*1);
            cout<<thickness[i]<<" ";
        }
        cout<<"."<<endl;
    }

    if (packages.size()==0)
    {
        for (i=0;i<nStacks;i++)
        {
            packages.push_back(1);
        }
    }


//    //this is the number of directions we expect at the bval and bvecs file, including b0
//    cout<<"Number of diffusion weighted directions is "<<nStacks<<endl;
//    Array<Array<double> > directions(3,Array<double>(nStacks+1,0));
//    Array<double> bvalues(nStacks+1,0);
//
//    ifstream in(textfile);
//    int coord = 0;
//    int dir = 0;
//    double num;
//
//    cout<<"Reading directions: "<<endl;
//    if (in.is_open())
//    {
//        while (!in.eof())
//        {
//            in >> num;
//            if ((coord<4)&&(dir<nStacks))
//            {
//                if(coord<3)
//                    directions[coord][dir+1]=num;
//                else
//                    bvalues[dir+1]=num;
//            }
//            cout<<num<<" ";
//            coord++;
//            if (coord>=4)
//            {
//                coord=0;
//                dir++;
//                cout<<endl;
//            }
//        }
//        in.close();
//    }
//    else
//    {
//        cout << "Unable to open file " << textfile << endl;
//        exit(1);
//    }
//


    //Output volume
    RealImage reconstructed;

    //Set debug mode
    if (debug) reconstruction.DebugOn();
    else reconstruction.DebugOff();

    //Set force excluded slices
    reconstruction.SetForceExcludedSlices(force_excluded);

    //Set low intensity cutoff for bias estimation
    reconstruction.SetLowIntensityCutoff(low_intensity_cutoff)  ;


    // Check whether the template stack can be identified
    if (templateNumber<0)
    {
        cerr<<"Please identify the template by assigning id transformation."<<endl;
        exit(1);
    }
//    //If no mask was given and flag "remove_black_background" is false, try to create mask from the template image in case it was padded
//    if ((mask==NULL)&&(!remove_black_background))
//    {
//        mask = new RealImage(stacks[templateNumber]);
//        *mask = CreateMask(*mask);
//    }
    /*
     if (mask !=NULL)
     {
     //first resample the mask to the space of the stack
     //for template stack the transformation is identity
     RealImage m = *mask;
     reconstruction.TransformMask(stacks[templateNumber],m,stack_transformations[templateNumber]);
     //Crop template stack
     reconstruction.CropImage(stacks[templateNumber],m);
     if (debug)
     {
     m.Write("maskTemplate.nii.gz");
     stacks[templateNumber].Write("croppedTemplate.nii.gz");
     }
     }
     */
    //Create template volume with isotropic resolution
    //if resolution==0 it will be determined from in-plane resolution of the image

    //resolution = reconstruction.CreateTemplate(stacks[templateNumber],resolution);
    resolution = reconstruction.CreateTemplate(target,resolution);

    //Set mask to reconstruction object.
    reconstruction.SetMaskOrient(mask,orient);
    //reconstruction.SetMask(mask,0);
    reconstruction.SetRegulSteps(regul_steps);

    //to redirect output from screen to text files

    //to remember cout and cerr buffer
    streambuf* strm_buffer = cout.rdbuf();
    streambuf* strm_buffer_e = cerr.rdbuf();
    //files for registration output
    string name;
    name = log_id+"log-registration.txt";
    ofstream file(name.c_str());
    name = log_id+"log-registration-error.txt";
    ofstream file_e(name.c_str());
    //files for reconstruction output
    name = log_id+"log-reconstruction.txt";
    ofstream file2(name.c_str());
    name = log_id+"log-evaluation.txt";
    ofstream fileEv(name.c_str());
    name = log_id+"log-sh.txt";
    ofstream fileSH(name.c_str());

    //set precision
    cout<<setprecision(3);
    cerr<<setprecision(3);

    //perform volumetric registration of the stacks
    //redirect output to files
    if ( ! no_log ) {
        cerr.rdbuf(file_e.rdbuf());
        cout.rdbuf (file.rdbuf());
    }

    reconstruction.SetTemplateImage(stacks[0],orient);


    if(!have_stack_transformations) {

        reconstruction.StackRegistrations(stacks,stack_transformations);//,templateNumber);
    }

    cout<<endl;
    //redirect output back to screen
    if ( ! no_log ) {
        cout.rdbuf (strm_buffer);
        cerr.rdbuf (strm_buffer_e);
    }


    average = reconstruction.CreateAverage(stacks,stack_transformations);
//    if (debug)
        average.Write("average1.nii.gz");
    reconstruction.SetTemplate(average);
    reconstruction.MaskVolume();
    reconstructed = reconstruction.GetReconstructed();
    reconstructed.Write("image0.nii.gz");


    //Create slices and slice-dependent transformations
    reconstruction.CreateSlicesAndTransformations(stacks,stack_transformations,thickness);
    reconstruction.CreateSliceDirections(new_directions,new_bvalues);
    //reconstruction.SaveSlices();
    reconstruction.SetForceExcludedStacks(force_excluded_stacks);
    //calculate slice order
    int z = stacks[0].GetZ();
    int j,k;
    Array<int> slice_order;
    cout<<"Slices in stack: "<<z<<endl;
    for(i=0;i<stacks.size();i++) //go though all directions
    {
        for(k=0;k<z;k=k+2) //even
            slice_order.push_back(i*z+k);
        for(k=1;k<z;k=k+2) //odd
            slice_order.push_back(i*z+k);
    }

    //    for(i=0;i<slice_order.size();i++)
    //        cout<<slice_order[i]<<" ";
    cout<<"Number of slices: "<<slice_order.size() << endl;;
    reconstruction.SetSliceOrder(slice_order);
    //exit(1);


    if (intensity_exclusion) {
        reconstruction.StructuralExclusion(stack_transformations, *mask, stacks, intensity_threshold);
    }


    //Mask all the slices
    reconstruction.MaskSlices();


    //Set sigma for the bias field smoothing
    //if (sigma>0)
    reconstruction.SetSigma(sigma);

    //Set global bias correction flag
    if (global_bias_correction)
        reconstruction.GlobalBiasCorrectionOn();
    else
        reconstruction.GlobalBiasCorrectionOff();

    //if given read slice-to-volume registrations
    if (folder!=NULL)
        reconstruction.ReadTransformations(folder);


    //Initialise data structures for EM
    reconstruction.InitializeEM();

    //CHange: only packages and odd even for now
    //interleaved registration-reconstruction iterations
    //iterations=4;

    if(sh_only)
        reconstruction.SetSmoothingParameters(delta, lambda);
    else
        reconstruction.SetSmoothingParameters(delta, lambda*8);



    for (int iter=0;iter<iterations;iter++)
    {
        //only last iteration!!!
        if(sh_only)
            if(iter<iterations)
                continue;
        //Print iteration number on the screen
        if ( ! no_log ) {
            cout.rdbuf (strm_buffer);
        }
        cout<<"Iteration "<<iter<<". "<<endl;

        //perform slice-to-volume registrations - skip the first iteration
        if ((iter>0)&&(!sh_only))
        {
            if ( ! no_log ) {
                cerr.rdbuf(file_e.rdbuf());
                cout.rdbuf (file.rdbuf());
            }
            cout<<"Iteration "<<iter<<": "<<endl;

            if(iter<=1)
            {
                reconstruction.PackageToVolume(corrected_stacks,packages,stack_transformations);

            }
            else
            {
                reconstruction.SliceToVolumeRegistration();
            }

            if (iter>1)
            {
                if(motion_sigma>0)
                {
                    reconstruction.SetMotionSigma(motion_sigma);
                    if(motion_model_hs)
                        reconstruction.PostProcessMotionParametersHS2(stacks[templateNumber]);
                    else
                        reconstruction.PostProcessMotionParameters2(stacks[templateNumber]);
                }
            }

            reconstruction.UpdateSlices(stacks,thickness);
            reconstruction.MaskSlices();

            cout<<endl;
            if ( ! no_log ) {
                cerr.rdbuf (strm_buffer_e);
            }
        }


        //Write to file
        if ( ! no_log ) {
            cout.rdbuf (file2.rdbuf());
        }
        cout<<endl<<endl<<"Iteration "<<iter<<": "<<endl<<endl;

        //Set smoothing parameters
        //amount of smoothing (given by lambda) is decreased with improving alignment
        //     //delta (to determine edges) stays constant throughout
        if(iter==(iterations-1))
            reconstruction.SetSmoothingParameters(delta,lambda);//lastIterLambda);
        else
        {
            double l=lambda;
            for (i=0;i<levels;i++)
            {
                if (iter==iterations*(levels-i-1)/levels)
                    reconstruction.SetSmoothingParameters(delta, l);
                l*=2;
            }
        }

        //Use faster reconstruction during iterations and slower for final reconstruction
        if ( iter<(iterations-1) )
            reconstruction.SpeedupOn();
        else
            reconstruction.SpeedupOff();

        if(robust_slices_only)
            reconstruction.ExcludeWholeSlicesOnly();

        //Initialise values of weights, scales and bias fields
        reconstruction.InitializeEMValues();

        reconstruction.Set3DRecon();
        reconstruction.CoeffInit();

        reconstruction.GaussianReconstruction(0);

        //Simulate slices (needs to be done after Gaussian reconstruction)
        reconstruction.SimulateSlices();

        //Initialize robust statistics parameters
        reconstruction.InitializeRobustStatistics();

        //EStep
        if(robust_statistics)
            reconstruction.EStep();

        //number of reconstruction iterations
        if ( iter==(iterations-1) )
        {
            if(robust_statistics)
                rec_iterations = 10;
            else
                rec_iterations = 0;
        }
        else
            rec_iterations = 0;


        //reconstruction iterations
        if(iter==iterations)
            rec_iterations = 10;
        else
            rec_iterations = 10;

        i=0;
        for (i=0;i<rec_iterations;i++)
        {
            cout<<endl<<"  Reconstruction iteration "<<i<<". "<<endl;

            //if (i==15)
            //intensity_matching = true;

            if (intensity_matching)
            {
                //calculate bias fields
                if (sigma>0)
                    reconstruction.Bias();
                //calculate scales
                reconstruction.Scale();
            }

            //Update reconstructed volume
            reconstruction.Superresolution(i+1);

            if (intensity_matching)
            {
                //if((sigma>0)&&(!global_bias_correction))
                reconstruction.NormaliseBias(i);
            }

            // Simulate slices (needs to be done
            // after the update of the reconstructed volume)
            reconstruction.SimulateSlices();

            if(robust_statistics)
                reconstruction.MStep(i+1);

            //E-step
            if(robust_statistics)
                reconstruction.EStep();

            //Save intermediate reconstructed image
            if (debug)
            {
                reconstructed=reconstruction.GetReconstructed();
                sprintf(buffer,"super%i.nii.gz",i);
                reconstructed.Write(buffer);
            }


        }//end of reconstruction iterations

        //Mask reconstructed image to ROI given by the mask
        if(!bspline)
            reconstruction.MaskVolume();

        //Save reconstructed image
        //if (debug)
        //{
        reconstructed=reconstruction.GetReconstructed();
        sprintf(buffer,"image%i.nii.gz",iter);
        reconstructed.Write(buffer);
        //reconstruction.SaveConfidenceMap();
        //}

        //Evaluate - write number of included/excluded/outside/zero slices in each iteration in the file
        if ( ! no_log ) {
            cout.rdbuf (fileEv.rdbuf());
        }
        reconstruction.Evaluate(iter);
        cout<<endl;

        if ( ! no_log ) {
            cout.rdbuf (strm_buffer);
        }

    }// end of interleaved registration-reconstruction iterations



    //save final result
    //reconstruction.RestoreSliceIntensities();
    //reconstruction.ScaleVolume();
    /*
     reconstructed=reconstruction.GetReconstructed();
     reconstructed.Write(output_name);
     reconstruction.SaveTransformations();
     reconstruction.SaveSlices();

     if ( info_filename.length() > 0 )
     reconstruction.SlicesInfo( info_filename.c_str(),
     stack_files );

     if(debug)
     {
     reconstruction.SaveWeights();
     reconstruction.SaveBiasFields();
     //reconstruction.SaveConfidenceMap();
     reconstruction.SimulateStacks(stacks);
     for (unsigned int i=0;i<stacks.size();i++)
     {
     sprintf(buffer,"simulated%i.nii.gz",i);
     stacks[i].Write(buffer);
     }
     }

     */

    corrected_stacks.clear();
    for (unsigned int i=0;i<stacks.size();i++)
    {
        corrected_stacks.push_back(stacks[i]);
    }

    reconstruction.CorrectStackIntensities(corrected_stacks);

    for (unsigned int i=0;i<stacks.size();i++)
    {
        sprintf(buffer,"correctedstack%i.nii.gz",i);
        corrected_stacks[i].Write(buffer);
    }


    reconstruction.UpdateSlices(corrected_stacks, thickness);
    Array<RealImage> old_bias;
    Array<double> old_scale;
    reconstruction.ResetBiasAndScale(old_bias,old_scale);





    /////////////////////////////////////////////////
    //Beginning of SH part

    cerr.rdbuf(fileSH.rdbuf());
    cout.rdbuf (fileSH.rdbuf());



    reconstruction.SetSmoothingParameters(delta, lambda);


    /*
     if(recon_1D)
     reconstruction.Set1DRecon();
     else
     reconstruction.Set3DRecon();

     if(iterations==0)
     reconstruction.InitializeEMValues();
     reconstruction.CoeffInit();
     reconstruction.GaussianReconstruction4D(nStacks);
     //reconstruction.SaveBiasFields();
     //exit(1);
     */

    /////////TESTING THIS
    // Reset intensity matching and robust statistics
    reconstruction.InitializeEMValues();
    reconstruction.InitializeRobustStatistics();
    /////////////TESTING this

    reconstruction.Init4D(nStacks);
    //directions
    int nDir = nStacks;
    Matrix dirs_xyz(nDir,3);
    for(int i=0;i<nDir;i++)
        for(int j=0;j<3;j++)
            dirs_xyz(i,j) = new_directions[j][i+1];
    dirs_xyz.Print();

    //SH reconstruction
    reconstruction.SetLambdaLB(lambdaLB);
    //TEST
    //reconstruction.SetLambdaLB(1);//to create order=2 effectively

    reconstruction.InitSH(dirs_xyz,order);

    //reconstruction.Set3DRecon();
    //reconstruction.CoeffInit();
    reconstruction.SimulateSlicesDTI();
    //reconstruction.SaveSimulatedSlices();

    //TEST
    //reconstruction.SetSigma(10);

    for(int sh_iter=1; sh_iter<=1;sh_iter++)
    {

        for(int iteration = 0; iteration < sr_sh_iterations; iteration++ )
        {
            //TEST
            //if(iteration == 10)
            //{
            //reconstruction.SetLambdaLB(0);
            //reconstruction.SimulateSignal();
            //reconstruction.WriteSimulatedSignal("_simulated_signal_10it.nii.gz");
            //}


            if (intensity_matching_sh)
            {
                //calculate bias fields
                if (sigma>0)
                    reconstruction.Bias();
                //calculate scales
                reconstruction.Scale();
                //reconstruction.NormaliseBiasDTI(iteration,stack_transformations,order);
            }

            if(lambda>0)
                reconstruction.SuperresolutionDTI(iteration,true,sh_alpha);
            else
                reconstruction.SuperresolutionDTI(iteration,false,sh_alpha);

            if (intensity_matching_sh)
            {
                //if(sigma>0)
                reconstruction.NormaliseBiasSH(iteration);
            }

            reconstruction.SimulateSlicesDTI();
            cout.rdbuf (strm_buffer);
            cout<<"SH iter "<<iteration<<": consistency = "<<reconstruction.Consistency()<<endl;
            cout.rdbuf (fileSH.rdbuf());

            if(robust_statistics)
                reconstruction.MStep(i+1);

            //E-step
            if(robust_statistics)
                reconstruction.EStep();
            reconstruction.SaveSHcoeffs(iteration);
            /*
             if(robust_statistics)
             reconstruction.MStep(i+1);
             //E-step
             if(robust_statistics)
             reconstruction.EStep();
             */

        }
        if(sh_iter==0)
        {
            reconstruction.SliceToVolumeRegistrationSH();

            if(motion_model_hs)
                reconstruction.PostProcessMotionParametersHS(stacks[templateNumber]);
            else
                reconstruction.PostProcessMotionParameters(stacks[templateNumber]);

        }

    }


    ///////////////////
    //end of SH part


    reconstruction.SlicesInfo2(stack_transformations, *mask);



    if (debug) {
        reconstruction.SaveSimulatedSlices();
        reconstruction.SaveWeights();
    }

    reconstruction.SimulateSignal();//dirs_xyz,order);

    if (debug) {
        reconstruction.SaveSlices();
        reconstruction.SaveTransformations();
    }



    for (unsigned int i=0;i<stacks.size();i++)
    {
        sprintf(buffer,"origstack%i.nii.gz",i);
        stacks[i].Write(buffer);
    }


    //reconstruction.SimulateStacksDTI(stacks,true);
    double consistency = reconstruction.Consistency();
    cout<<"Consistency = "<<consistency<<endl;

    if ( ! no_log ) {
        cout.rdbuf (fileEv.rdbuf());
    }
    reconstruction.Evaluate(10);
    cout<<endl;
    cout<<"Consistency = "<<consistency<<endl;

    if ( ! no_log ) {
        cout.rdbuf (strm_buffer);
    }


    corrected_stacks.clear();
    for (unsigned int i=0;i<stacks.size();i++)
    {
        corrected_stacks.push_back(stacks[i]);
    }

    reconstruction.SimulateStacksDTI(stacks,false);
    //  reconstruction.SimulateStacksDTIIntensityMatching(stacks,true);
    for (unsigned int i=0;i<stacks.size();i++)
    {
        sprintf(buffer,"simulated-exc%i.nii.gz",i);
        stacks[i].Write(buffer);
    }

    //reconstruction.AddBiasAndScale(old_bias, old_scale);
    reconstruction.SimulateStacksDTIIntensityMatching(stacks,true);
    for (unsigned int i=0;i<stacks.size();i++)
    {
        sprintf(buffer,"simulated%i.nii.gz",i);
        stacks[i].Write(buffer);
    }


    reconstruction.CorrectStackIntensities(corrected_stacks);
    for (unsigned int i=0;i<stacks.size();i++)
    {
        sprintf(buffer,"correctedstacksh%i.nii.gz",i);
        corrected_stacks[i].Write(buffer);
    }


    cout << "Reconstructed DWI signal: " << output_name << endl;
    RealImage final_simulated_signal = reconstruction.ReturnSimulatedSignal();
    final_simulated_signal.Write(output_name);



    return 0;

}
