#include <getopt.h>
#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <sys/stat.h>

#include <TCanvas.h>
#include <TFile.h>
#include <TKey.h>
#include <TROOT.h>
#include <TF1.h>

#include <TASImage.h>
#include <TString.h>

#include <TApplication.h>
#include <TStyle.h>
#include <TSystem.h>

#include <RootTools.h>

#include <iostream>
#define PR(x) std::cout << "++DEBUG: " << #x << " = |" << x << "| (" << __FILE__ << ", " << __LINE__ << ")\n";

int flag_png;
int flag_eps;
int flag_pdf;

int flag_width = 800;
int flag_height = 600;

std::string outpath = "./";

TDirectory * target;

struct CanvasCfg
{
// 	std::string cn;
	int cnt;
	int w;
	int h;
};

enum FilterState { FS_None, FS_Modify, FS_Exclusive };
FilterState global_filter = FS_None;

typedef std::map<std::string, CanvasCfg> FilterMap;
FilterMap global_map;

TString filter = "";

unsigned int counter = 0;

inline bool file_exists(const std::string & file)
{
	struct stat buffer;
	return (stat(file.c_str(), &buffer) == 0);
}

void exportimg(TObject * obj, TDirectory * dir, const CanvasCfg & ccfg)
{
	TCanvas * can = nullptr;

	dir->GetObject(obj->GetName(), can);

	can->Draw();
	can->SetCanvasSize(ccfg.w, ccfg.h);
	std::cout << "Exporting " << can->GetName() << std::endl;

	if (flag_png) RootTools::ExportPNG((TCanvas*)can, outpath);
	if (flag_eps) RootTools::ExportEPS((TCanvas*)can, outpath);
	if (flag_pdf) RootTools::ExportPDF((TCanvas*)can, outpath);
	++counter;
}

void browseDir(TDirectory * dir, FilterState & fs, const FilterMap & filter_map)
{
	TKey * key;
	TIter nextkey(dir->GetListOfKeys());
	while ((key = (TKey*)nextkey()))
	{
		TObject *obj = key->ReadObj();

		if (obj->InheritsFrom("TDirectory"))
		{
			TDirectory * dir = (TDirectory*)obj;
			browseDir(dir, fs, filter_map);
		}
		else
		if (obj->InheritsFrom("TCanvas"))
		{
			CanvasCfg ccfg = { 1, flag_width, flag_height };
			FilterMap::const_iterator fit = filter_map.find(obj->GetName());
			bool found_fit = ( fit != filter_map.end() );

			if (fs == FS_Exclusive)
			{
				if (!found_fit)
					continue;

				if (fit->second.cnt <= 0)
					continue;

				ccfg = fit->second;
				exportimg(obj, dir, ccfg);
			}
			else if (fs == FS_Modify)
			{
				if (found_fit)
				{
					if (fit->second.cnt < 0)
						continue;
				}

				ccfg = fit->second;
				exportimg(obj, dir, ccfg);
			}
			else
			{
				exportimg(obj, dir, ccfg);
			}
		}
	}
}

FilterState parser(const std::string & fname, FilterMap & local_fm)
{
	// local FilterMap to be add to global one;
	std::string buff;
	std::string inline_name;
	bool parsing_local = false;

	// local setting for current file
	CanvasCfg local_cancfg = { 1, flag_width, flag_height };

	FilterState fs = FS_Modify;

	if (file_exists(fname))
	{
		std::cout << "Parsing config file " << fname << std::endl;
		std::ifstream local_fm_file(fname.c_str());

		while (!local_fm_file.eof())
		{
			std::getline(local_fm_file, buff);
			if (!buff.length())
				continue;

			// convert tabs into spaces
			size_t tab_pos = 0;
			while (tab_pos = buff.find_first_of('\t', tab_pos), tab_pos != buff.npos)
			{
				buff.replace(tab_pos, 1, " ");
			}

			// find firts non-white character in the line
			// it should be hist name or '*'
			size_t pos_name = buff.find_first_not_of(" \t", 0);
			if (pos_name == buff.npos)
				continue;

			size_t pos = buff.find_first_of(" \t-", pos_name);
			inline_name = buff.substr(pos_name, pos - pos_name).c_str();

			parsing_local = (inline_name == "*");

			// inline setting for current line
			CanvasCfg inline_cancfg = local_cancfg;

			// parse rest of the line to search for w, h, -
			while (pos != buff.npos)
			{
				pos = buff.find_first_not_of(" \t", pos);

				char test_char = buff[pos];

				if (test_char == '-')
				{
					if (parsing_local)
						fs = FS_Exclusive;
					else
						inline_cancfg.cnt = -99;

					++pos;
				}
				else if (test_char == 'w')
				{
					if (buff[pos+1] == '=')
					{
						int old_pos = pos+2;
						pos = buff.find_first_not_of("0123456789", pos+2);
						std::string number = buff.substr(old_pos, pos - old_pos);
						int val_tmp = atoi(number.c_str());
						if (val_tmp)
								inline_cancfg.w = val_tmp;
					}
					else
					{
						std::cerr << "Parsing error, breaking parser" << std::endl;
						continue;
					}
				}
				else if (test_char == 'h')
				{
					if (buff[pos+1] == '=')
					{
						int old_pos = pos+2;
						pos = buff.find_first_not_of("0123456789", pos+2);
						std::string number = buff.substr(old_pos, pos - old_pos);
						int val_tmp = atoi(number.c_str());
						if (val_tmp)
							inline_cancfg.h = val_tmp;
					}
					else
					{
						std::cerr << "Parsing error, breaking parser" << std::endl;
						continue;
					}
				}
				else
				{
					if (pos == buff.npos)
						break;

					std::cerr << "Parsing error at:" << std::endl << " " << buff << std::endl;
					std::fill_n(std::ostream_iterator<char>(std::cerr), pos, ' ');
					std::cerr << "^" << std::endl << " breaking parser" << std::endl;
					break;
				}
			}

			if (parsing_local)
			{
				local_cancfg.w = inline_cancfg.w;
				local_cancfg.h = inline_cancfg.h;
			}

			local_fm[inline_name] = inline_cancfg;
		}
	}

// 	FilterMap & total_map = global_map;
// 	for (FilterMap::const_iterator it = global_map.begin(); it != global_map.end(); ++it)
// 		total_map[it->first] = it->second;

	if (!local_fm.size())
		return FS_None;

	return fs;
}

bool extractor(const std::string & file)
{
	TFile * f = TFile::Open(file.c_str(), "READ");

	if (!f->IsOpen())
	{
		std::cerr << "File " << file << " not open!" << std::endl;
		return false;
	}

	TSystem sys;

	std::string dir_name = sys.DirName(file.c_str());
	std::string file_basename = sys.BaseName(file.c_str());

	size_t last_dot = file_basename.find_last_of('.');
// 	size_t ext_len = file_basename.end() - last_dot;
	std::string imgcfg_name = dir_name + "/." + file_basename.replace(last_dot, std::string::npos, ".iecfg");

	FilterState local_filter = global_filter;
	FilterMap local_map = global_map;

	if (!global_map.size())
	{
		local_filter = parser(imgcfg_name, local_map);
	}

	std::cout << "Maps summary for mode " << local_filter << std::endl;
	FilterMap & total_map = local_map;

	for (FilterMap::const_iterator it = total_map.begin(); it != total_map.end(); ++it)
	{
		if (it->second.cnt > 0)
			std::cout << " " << it->first << " [" << it->second.cnt << "] w = " << it->second.w << " h = " << it->second.h << std::endl;
		else
			std::cout << " " << it->first << " [" << it->second.cnt << "] " << std::endl;
	}

	f->cd();

	browseDir(f, local_filter, local_map);

	std::cout << "Total: " << counter << std::endl;
	return true;
}

int main(int argc, char ** argv) {
	TROOT AnalysisDST_Cal1("TreeAnalysis","compiled analysisDST macros");
	TApplication app("treeanal", NULL, NULL, NULL, 0);
	gROOT->SetBatch();

	struct option lopt[] =
		{
			{"png",			no_argument,		&flag_png,	1},
			{"eps",			no_argument,		&flag_eps,	1},
			{"pdf",			no_argument,		&flag_pdf,	1},
			{"dir",			required_argument,	0,		'd'},
			{"width",		required_argument,	0,		'w'},
			{"height",		required_argument,	0,		'h'},
			{"filter",		required_argument,	0,		'f'},
			{ 0, 0, 0, 0 }
		};


	Int_t c = 0;
	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "d:w:h:f:", lopt, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 0:
				if (lopt[option_index].flag != 0)
					break;
				printf ("option %s", lopt[option_index].name);
				if (optarg)
					printf (" with arg %s", optarg);
				printf ("\n");
				break;
			case 'd':
				outpath = optarg;
				break;
			case 'w':
				flag_width = atoi(optarg);
				break;
			case 'h':
				flag_height = atoi(optarg);
				break;
			case 'f':
				global_filter = FS_Exclusive;
				{
					CanvasCfg cc = { 99, flag_width, flag_height };
					global_map[optarg] = cc;
				}
				break;
			case '?':
// 				Usage();
				exit(EXIT_SUCCESS);
				break;
//			case '?':
// 				abort();
//				break;
			default:
				break;
		}
	}

	target = gDirectory;

	RootTools::NicePalette();
	RootTools::MyMath();

	if ( ! (flag_png | flag_eps | flag_pdf) )
		flag_png = 1;

	if (global_map.size())
	{
		std::cout << "Filtering for :" << std::endl;
		for (FilterMap::iterator it = global_map.begin(); it != global_map.end(); ++it)
			std::cout << " " << it->first << std::endl;
	}

	while (optind < argc)
	{
		std::cout << "Extracting from " << argv[optind] << std::endl;
		extractor(argv[optind++]);
	}

	return 0;
}
