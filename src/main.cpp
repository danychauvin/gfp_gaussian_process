#include "CSVconfig.h"

#include "likelihood.h"
#include "minimizer_nlopt.h"

#include "tests.h"
#include <filesystem>
#include <iostream> 
#include <iterator> 
#include <iomanip> 


void run_minimization(std::vector<MOMAdata> &cells, Parameter_set &params, 
                      std::map<std::string, std::string> arguments){
    std::cout << "-> Minimizaton" << "\n";
    init_cells(cells, 5);

    /* set and setup (global) output file */
    _outfile_ll = outfile_name_minimization(arguments, params);
    setup_outfile_likelihood(_outfile_ll, params);
    std::cout << "Outfile: " << _outfile_ll << "\n";

    /* minimization for tree starting from cells[0] */
    minimize_wrapper(&total_likelihood, cells, params, std::stod(arguments["rel_tol"] ));
}


void run_bound_1dscan(std::vector<MOMAdata> &cells, Parameter_set params,
                      std::map<std::string, std::string> arguments){
    std::cout << "-> 1d Scan" << "\n";
    init_cells(cells, 5);
    
    for(size_t i=0; i<params.all.size(); ++i){
        if (params.all[i].bound){
            /* 
            reset params, if paramters have been minimized the final value will be taken
            otherwise the init values are taken 
            */
            std::vector<double> params_vec = params.get_final();
             
            /* 
            set and setup new (global) output file, for each scan, 
            filename containing the parameter name that is varied
            */
            _outfile_ll = outfile_name_scan(arguments, params.all[i].name);
            setup_outfile_likelihood(_outfile_ll, params);
            std::cout << "Outfile: " << _outfile_ll << "\n";

            /* set sampling vector np.arange style*/
            std::vector<double> sampling = arange<double>(params.all[i].lower, 
                                                            params.all[i].upper, 
                                                            params.all[i].step);
            for(size_t j=0; j<sampling.size(); ++j){
                params_vec[i] = sampling[j];
                total_likelihood(params_vec, cells);
            }
        }
    }
}


void run_prediction(std::vector<MOMAdata> &cells, Parameter_set params, 
                    std::map<std::string, std::string> arguments){
    std::cout << "-> prediction" << "\n";
    
    std::string outfile = outfile_name_prediction(arguments);
    std::string outfile_b = outfile_name_prediction(arguments, "_backward");
    std::string outfile_f = outfile_name_prediction(arguments, "_forward");


    std::cout << "Outfile: " << outfile << "\n";
    std::cout << "Outfile backward: " << outfile_b << "\n";
    std::cout << "Outfile forward: " << outfile_f << "\n";


    std::vector<double> params_vec = params.get_final();

    /* forward...*/
    init_cells(cells, 5);
    prediction_forward(params_vec, cells);

    /* backward...*/
    init_cells_r(cells, 5);
    prediction_backward(params_vec, cells);

    /* combine the two */
    combine_predictions(cells);

    /* save */
    write_pretictions_to_file(cells, outfile_b, params, "b");
    write_pretictions_to_file(cells, outfile_f, params, "f");

    write_pretictions_to_file(cells, outfile, params);
}


std::map<std::string, std::string> arg_parser(int argc, char** argv){
    std::vector<std::vector<std::string>> keys = {
        {"-h","--help", "help message"},
        {"-i", "--infile", "(required) input/data file"},
        {"-b", "--parameter_bounds", "(required) file defining the type, step, bounds of the parameters"},
        {"-c", "--csv_config", "file that sets the colums that will be used from the input file"},
        {"-l","--print_level", "print level >=0, default=0"},
        {"-o","--outdir", "specify output direction and do not use default"},
        {"-r","--rel_tol", "relative tolerance of maximization, default=1e-2"},
        {"-m","--maximize", "run maximization"},
        {"-s","--scan", "run 1d parameter scan"},
        {"-p","--predict", "run prediction"}
        };

    std::map<std::string, int> key_indices; 
    for (size_t i = 0; i < keys.size(); ++i){
        key_indices.insert(std::pair<std::string, int>(keys[i][0], i)); 
    }

    std::map<std::string, std::string> arguments;
    /* defaults: */
    arguments["print_level"] = "0";
    arguments["rel_tol"] = "1e-2";

    for(int k=0; k<keys.size(); ++k){
        for(int i=1; i<argc ; ++i){
            if (argv[i] == keys[k][0] || argv[i] == keys[k][1]){
                 if(k==key_indices["-i"]) 
                    arguments["infile"] = argv[i+1];
                else if(k==key_indices["-b"]) 
                    arguments["parameter_bounds"] = argv[i+1];
                else if(k==key_indices["-c"]) 
                    arguments["csv_config"] = argv[i+1];
				else if(k==key_indices["-l"])
                    arguments["print_level"] = argv[i+1];
				else if(k==key_indices["-o"])
                    arguments["outdir"] = argv[i+1];
				else if(k==key_indices["-r"])
                    arguments["rel_tol"] = argv[i+1];
                else if(k==key_indices["-m"])
                    arguments["minimize"] = "1";
                else if(k==key_indices["-s"])
                    arguments["scan"] = "1";
                else if(k==key_indices["-p"])
                    arguments["predict"] = "1";
                else if (k==key_indices["-h"]){
                    arguments["quit"] = "1";
                    std::cout << "Usage: ./gfp_gaussian <infile> [-options]\n";
                    for(size_t j=0; j<keys.size(); ++j)
                        std::cout << pad_str(keys[j][0] + ", "+ keys[j][1], 27) << keys[j][2] <<"\n";
                }
            }
        }
    }
    /* Check is required filenames are parsed and files exist */
    if (!arguments.count("infile")){
        std::cout << "Required infile flag not set!\n";
        arguments["quit"] = "1";
    }
    else if(! std::filesystem::exists(arguments["infile"])){
        std::cout << "Infile " << arguments["infile"] << " not found (use '-h' for help)!" << std::endl;
        arguments["quit"] = "1";
    }

    if (!arguments.count("parameter_bounds")){
        std::cout << "Required parameter_bounds flag not set!\n";
        arguments["quit"] = "1";
    }
    else if(! std::filesystem::exists(arguments["parameter_bounds"])){   
        std::cout << "Paramters bound file " << arguments["parameter_bounds"] << " not found (use '-h' for help)!" << std::endl;
        arguments["quit"] = "1";
    }

    /* Check if csv file (if parsed) exists, to avoid confusion */
    if(arguments.count("csv_config") && !std::filesystem::exists(arguments["csv_config"])){   
        std::cout << "csv_config flag set, but csv configuration file " << arguments["csv_config"] << " not found!" << std::endl;
        arguments["quit"] = "1";
    }
    return arguments;
}


int main(int argc, char** argv){
    // test_prediction();

    /* process command line arguments */
    std::map<std::string, std::string> arguments = arg_parser(argc, argv);
    _print_level = std::stoi(arguments["print_level"]);

    if (arguments.count("quit")){
        std::cout << "Quit\n";
        return 0;    
    }

    /* get parameter and csv config file */
    Parameter_set params(arguments["parameter_bounds"]);
    std::cout << params << "\n";

    CSVconfig config(arguments["csv_config"]);
    std::cout << config << "\n";

    /* Read data from input file */
    std::cout << "-> Reading" << "\n";
    std::vector<MOMAdata> cells =  getData(arguments["infile"], 
                                            config.time_col,
                                            config.divide_time,
                                            config.length_col,
                                            config.length_islog,
                                            config.fp_col,
                                            config.delm,
                                            config.cell_tags,
                                            config.parent_tags);
    if (!cells.size()){
        std::cout << "Quit\n";
        return 0;    
    }
    /* genealogy built via the parent_id (string) given in data file */
    build_cell_genealogy(cells);



    /* run bound_1dscan, minimization and/or prediction... */
    if (arguments.count("minimize"))
        run_minimization(cells, params, arguments);

    if (arguments.count("scan"))
        run_bound_1dscan(cells, params, arguments);

    if (arguments.count("predict"))
        run_prediction(cells, params, arguments);

    std::cout << "Done." << std::endl;
    return 0;
}


