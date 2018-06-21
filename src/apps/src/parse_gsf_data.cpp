#include <cereal/archives/json.hpp>

#include <cxxopts.hpp>
#include <data_tools/gsf_data.h>
#include <data_tools/transforms.h>

#include <gpgs_slam/igl_visualizer.h>

#include <sparse_gp/sparse_gp.h>
#include <sparse_gp/rbf_kernel.h>
#include <sparse_gp/gaussian_noise.h>

using namespace std;

int main(int argc, char** argv)
{
    string folder_str;
    string sounds_file_str;
    string poses_file_str;
    string file_str;
	double lsq = 2.;
	double sigma = 1.;
	double s0 = .5;

	cxxopts::Options options("MyProgram", "One line description of MyProgram");
	options.add_options()
      ("help", "Print help")
      ("swaths", "Input gsf mb swaths folder", cxxopts::value(folder_str))
      ("sounds", "Input sound speed file", cxxopts::value(sounds_file_str))
      ("poses", "Input nav file", cxxopts::value(poses_file_str))
      ("file", "Output file", cxxopts::value(file_str))
      ("lsq", "RBF length scale", cxxopts::value(lsq))
      ("sigma", "RBF scale", cxxopts::value(sigma))
      ("s0", "Measurement noise", cxxopts::value(s0));

    auto result = options.parse(argc, argv);
	if (result.count("help")) {
        cout << options.help({"", "Group"}) << endl;
        exit(0);
	}
    if (result.count("swaths") == 0 || result.count("sounds") == 0 || result.count("poses") == 0) {
		cout << "Please provide input swaths, sounds and poses args..." << endl;
		exit(0);
    }
    if (result.count("file") == 0) {
		cout << "Please provide output file arg..." << endl;
		exit(0);
    }
	
	boost::filesystem::path folder(folder_str);
    boost::filesystem::path sounds_path(sounds_file_str);
    boost::filesystem::path poses_path(poses_file_str);
    boost::filesystem::path path(file_str);

	cout << "Input folder : " << folder << endl;
	cout << "Output file : " << path << endl;
    gsf_mbes_ping::PingsT pings = parse_folder<gsf_mbes_ping>(folder);

    gsf_nav_entry::EntriesT entries = parse_file<gsf_nav_entry>(poses_path);
    
    gsf_sound_speed::SpeedsT speeds = parse_file<gsf_sound_speed>(sounds_path);

    match_sound_speeds(pings, speeds);
    mbes_ping::PingsT new_pings = convert_matched_entries(pings, entries);

    gp_submaps ss;
    tie(ss.points, ss.trans, ss.angles, ss.matches, ss.bounds) = create_submaps(new_pings);

    for (int i = 0; i < ss.points.size(); ++i) {

        //clip_submap(ss.points[i], ss.bounds[i], minx, maxx);

        gp_submaps::ProcessT gp(100, s0);
        gp.kernel.sigmaf_sq = sigma;
        gp.kernel.l_sq = lsq*lsq;
        gp.kernel.p(0) = gp.kernel.sigmaf_sq;
        gp.kernel.p(1) = gp.kernel.l_sq;
        // this will also centralize the points
        Eigen::MatrixXd X = ss.points[i].leftCols(2);
        Eigen::VectorXd y = ss.points[i].col(2);
        //gp.train_parameters(X, y);
        gp.add_measurements(X, y);

        std::cout << "Done training gaussian process..." << std::endl;
        ss.gps.push_back(gp);
        cout << "Pushed back..." << endl;

        ss.rots.push_back(euler_to_matrix(ss.angles[i](0), ss.angles[i](1), ss.angles[i](2)));
    }
	
    IglVisCallback vis(ss.points, ss.gps, ss.trans, ss.angles, ss.bounds);
    vis.display();

    write_data(ss, path);

    return 0;
}

