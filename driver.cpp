/*

		A utility for concatenating power results data
		
		Kevin Crepps 2022

*/

#include <iostream>
#include <sstream>
#include <fstream>
#include <shlobj.h>
#include <conio.h>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <limits>
#include <Windows.h>
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

// Prototypes
std::string Caps(std::string);

bool GetFiles();

std::string GetPath(std::string);

bool SortByValue(const std::pair<int, float>&, const std::pair<int, float>&);

bool IsolateHighest();

bool DetermineTightest();

void Push3(int, int);

bool StoreData();

bool WriteData();

// Structure definitions
struct Average
{
	float aValue[3];

	Average() { aValue[0] = -999.0f; aValue[1] = -999.0f; aValue[2] = -999.0f; }
};

// Globals
std::vector<std::string> g_vAvailable;

std::vector<std::vector<std::string>> g_vFilePaths;

std::vector<std::vector<std::string>> g_vSignals;

std::vector<std::vector<Average>> g_vAverages;

std::string g_xWorkingDirectory = "";

const std::string g_axKPIs[] = { "CS", "Idle", "ICOB", "1080p", "2160p", "Netflix", "Teams", "MM25"};

std::vector<int> g_vKPIIndices;

std::vector<float> g_vGroup;

std::vector<std::vector<float>> g_vCombinations;

std::unordered_map<int, float> g_averageMap;

int g_nCurrentKPI = 0;

bool g_bDebug = false,
	g_bAuto = false;


int main(int argc, char** argv)
{
	system("title Concatenate Summary Files && cls");

	g_xWorkingDirectory = argv[0];

	g_xWorkingDirectory = g_xWorkingDirectory.substr(0, g_xWorkingDirectory.size() - 12);

	if (argc == 1);

	else if (argc == 2)
	{
		if (Caps(argv[1]) == "-DEBUG")
			g_bDebug = true;

		else if (Caps(argv[1]) == "-AUTO")
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);

			if (g_bDebug)
				std::cout << "(DEBUG) Auto directory: " << g_xWorkingDirectory << "\n";

			g_bAuto = true;
		}

		else
		{
			std::cout << "Invalid argument supplied.\n";

			return EXIT_FAILURE;
		}
	}

	else
	{
		std::cout << "Invalid arguments supplied.\n";

		return EXIT_FAILURE;
	}

	if (GetFiles())
	{
		for (int i = 0; i < g_vKPIIndices.size(); ++i)
		{
			if (IsolateHighest()/*DetermineTightest()*/)
			{
				StoreData();

				++g_nCurrentKPI;
			}

			else
				return EXIT_FAILURE;
		}

		WriteData();
	}

	return EXIT_SUCCESS;
}

std::string Caps(std::string arg)
{
	std::string buffer = arg;

	for (int i = 0; i < buffer.size(); ++i)
		buffer[i] = toupper(buffer[i]);

	return buffer;
}

bool GetFiles()
{
	std::vector<std::string> vDirContents, vSubdirectories, vKeywords, vFilePaths;

	std::stringstream ss;

	std::string fileName, prefix, check;

	char cSelection = '?';

	int nSelection = -1;

	bool* abSelections = NULL, bSelected = false;

	check = char(251);

	if (!g_bAuto)
	{
		std::cout << "\nSpecify power results directory...\n\n"
			<< "(A)uto, (B)rowse or (E)nter manually?\n\n";

		while (toupper(cSelection) != 'A' && toupper(cSelection) != 'B' && toupper(cSelection) != 'E')
			cSelection = _getch();

		if (toupper(cSelection) == 'A')
		{
			ShowWindow(GetConsoleWindow(), SW_HIDE);

			g_bAuto = true;
		}

		else if (toupper(cSelection) == 'B')
		{
			Sleep(500);

			g_xWorkingDirectory = GetPath("Select power results folder...");

			while (g_xWorkingDirectory == "")
			{
				std::cout << "\nDirectory selection failed. Press any key to try again.\n";

				_getch();

				g_xWorkingDirectory = GetPath("Select power results folder...");
			}
		}

		else
		{
			std::cout << "Enter path: ";

			std::getline(std::cin, g_xWorkingDirectory);
		}
	}

	// Store directory contents
	for (const auto& entry : fs::directory_iterator(g_xWorkingDirectory))
	{
		ss.clear();
		ss.str("");

		ss << entry.path();

		vDirContents.push_back(ss.str());

		if (g_bDebug)
			std::cout << "(DEBUG) vDirContents.push_back: " << vDirContents[vDirContents.size() - 1] << "\n";
	}

	// Determine available KPIs
	for (int i = 0; i < vDirContents.size(); ++i)
	{
		// If directory contains 4k
		if (Caps(vDirContents[i]).find("4K") != std::string::npos)
		{
			// If available KPI vector doesn't already include 2160p
			if (std::find(g_vAvailable.begin(), g_vAvailable.end(), "2160p") == g_vAvailable.end())
				g_vAvailable.push_back("2160p");
		}

		else
		{
			for (int j = 0; j < 8; ++j)
			{
				// If directory contains KPI keyword
				if (Caps(vDirContents[i]).find(Caps(g_axKPIs[j])) != std::string::npos)
				{
					// If available KPI vector doesn't already include keyword
					if (std::find(g_vAvailable.begin(), g_vAvailable.end(), g_axKPIs[j]) == g_vAvailable.end())
					{
						g_vAvailable.push_back(g_axKPIs[j]);

						if (g_bDebug)
							std::cout << "(DEBUG) Adding available KPI: " << g_axKPIs[j] << "\n";
					}

					break;
				}
			}
		}
	}

	// Exit if no summary folders in working directory
	if (!g_vAvailable.size())
	{
		MessageBox(GetConsoleWindow(), "No power results found in current directory.", "Aborting...", MB_OK);

		return false;
	}

	if (!g_bAuto)
	{
		// Get KPI selection from user
		abSelections = new bool[g_vAvailable.size()];

		for (int i = 0; i < g_vAvailable.size(); ++i)
			abSelections[i] = false;

		system("cls");

		std::cout << "\nSelect KPIs:\n\n";

		for (int i = 0; i < g_vAvailable.size(); ++i)
		{
			std::cout << "(" << (i + 1) << ") " << g_vAvailable[i] << "\n";
		}

		std::cout << "\nPress enter to continue...";

		while ((nSelection < 0 || nSelection > g_vAvailable.size() - 1) || nSelection != -36)
		{
			nSelection = _getch() - '0' - 1;

			// Break on return press if at least one option selected
			bSelected = false;

			if (nSelection == -36)
			{
				for (int i = 0; i < g_vAvailable.size(); ++i)
				{
					if (abSelections[i])
						bSelected = true;
				}

				if (bSelected)
					break;
			}

			if (nSelection < g_vAvailable.size())
				abSelections[nSelection] = !abSelections[nSelection];

			system("cls");

			std::cout << "\nSelect KPIs:\n\n";

			for (int i = 0; i < g_vAvailable.size(); ++i)
			{
				std::cout << "(" << (i + 1) << ") " << g_vAvailable[i] << "   \t" << (abSelections[i] ? check : "") << "\n";
			}

			std::cout << "\nPress enter to continue...\n";
		}

		for (int i = 0; i < g_vAvailable.size(); ++i)
		{
			if (abSelections[i])
				vKeywords.push_back(g_vAvailable[i]);
		}
	}

	else if (g_bAuto)
	{
		for (int i = 0; i < g_vAvailable.size(); ++i)
			vKeywords.push_back(g_vAvailable[i]);
	}

	for (int i = 0; i < vKeywords.size(); ++i)
	{
		vSubdirectories.clear();

		// Store subdirectories w/ matching keywords
		for (int j = 0; j < vDirContents.size(); ++j)
		{
			ss.clear();
			ss.str("");

			ss << vDirContents[j];

			while (std::getline(ss, fileName, '\\'));

			if (g_bDebug)
				std::cout << "(DEBUG) Filename: " << fileName << "\n";

			ss.clear();
			ss.str("");

			ss << fileName;

			std::getline(ss, prefix, '_');

			// If directory item contains keyword and doesn't have a file extension
			if (Caps(prefix) == Caps(vKeywords[i]) || (Caps(vKeywords[i]) == "2160P" && Caps(prefix) == "4K"))// && fileName.find('.') == std::string::npos)
			{
				vSubdirectories.push_back(vDirContents[j]);

				if (g_bDebug)
					std::cout << "(DEBUG) Pushing subdirectory: " << vDirContents[j] << "\n";
			}
		}

		// Store summary file paths
		vFilePaths.clear();

		for (int j = 0; j < vSubdirectories.size(); ++j)
		{
			// Store directory contents
			for (const auto& entry : fs::directory_iterator(vSubdirectories[j]))
			{
				ss.clear();
				ss.str("");

				ss << entry.path();

				if (ss.str().find("summary.csv") != std::string::npos)
				{
					vFilePaths.push_back(ss.str());

					if (g_bDebug)
					{
						if (g_vFilePaths.size())
						{
							std::cout << "(DEBUG) vFiles.push_back: " << g_vFilePaths[g_nCurrentKPI][g_vFilePaths[g_nCurrentKPI].size() - 1] << "\n";
						}
					}
				}
			}
		}

		g_vFilePaths.push_back(vFilePaths);

		// Determine KPI for write column
		for (int j = 0; j < 8; ++j)
		{
			// If entered keyword contains KPI string
			if (Caps(vKeywords[i]).find(Caps(g_axKPIs[j])) != std::string::npos)
				g_vKPIIndices.push_back(j);

			if (Caps(vKeywords[i]).find("4K") != std::string::npos)
				g_vKPIIndices.push_back(4);
		}
	}

	// Free dynamically allocated memory
	if (abSelections)
		delete[] abSelections;

	return true;
}

std::string GetPath(std::string a_xTitle)
{
	BROWSEINFO bi = { 0 };

	LPITEMIDLIST list;

	TCHAR path[MAX_PATH] = "\0";

	bi.lpszTitle = a_xTitle.c_str();

	list = SHBrowseForFolder(&bi);

	if (SHGetPathFromIDList(list, path))
		return path;

	return "";
}

bool SortByValue(const std::pair<int, float>& a, const std::pair<int, float>& b)
{
	return (a.second < b.second);
}

bool IsolateHighest()
{
	std::ifstream file;

	std::string input;

	std::vector<std::pair<int, float>> pairs;

	std::vector<std::string> newPaths;

	// For each KPI
	for (int nCurrentKPI = 0; nCurrentKPI < g_vFilePaths.size(); ++nCurrentKPI)
	{
		g_averageMap.clear();

		if (g_vFilePaths[nCurrentKPI].size() > 3)
		{
			// For each file per KPI
			for (int i = 0; i < g_vFilePaths[nCurrentKPI].size(); ++i)
			{
				file.open(g_vFilePaths[nCurrentKPI][i], std::ios::in);

				if (file.is_open())
				{
					while (!file.eof())
					{
						// Read signal name
						getline(file, input, ',');

						// If current line contains MCP_TOTAL values
						if (input.find("P_MCP_TOTAL") != std::string::npos
							&& input.find("pmr") == std::string::npos)
						{
							// Ignore peak
							getline(file, input, ',');

							// Read and store average
							getline(file, input, ',');

							g_averageMap[i] = std::stof(input);

							break;
						}

						// If not, ignore the rest of the line
						else
							getline(file, input);
					}

					file.close();
				}

				else
				{
					std::cout << "Failed to open file when isolating three highest.\n";

					return false;
				}
			}

			// Push map values into vector of pairs
			for (int i = 0; i < g_averageMap.size(); ++i)
				pairs.push_back(std::make_pair(i, g_averageMap[i]));
			
			// Sort vector by value
			std::sort(pairs.begin(), pairs.end(), SortByValue);

			// Retain highest three
			while (pairs.size() > 3)
				pairs.erase(pairs.begin());

			// Update original vector
			for (int i = 0; i < 3; ++i)
				newPaths.push_back(g_vFilePaths[nCurrentKPI][pairs[i].first]);
			
			g_vFilePaths[nCurrentKPI] = newPaths;
		}
	}

	return true;
}

bool DetermineTightest()
{
	std::ifstream file;
	
	std::string input;

	float currentDistance, bestDifference(FLT_MAX);

	int comboIndex = -1;

	std::vector<std::string> newPaths;

	// For each KPI
	for (int nCurrentKPI = 0; nCurrentKPI < g_vFilePaths.size(); ++nCurrentKPI)
	{
		g_vGroup.clear();

		g_vCombinations.clear();

		g_averageMap.clear();

		if (g_vFilePaths[nCurrentKPI].size() > 3)
		{
			// For each file per KPI
			for (int i = 0; i < g_vFilePaths[nCurrentKPI].size(); ++i)
			{
				file.open(g_vFilePaths[nCurrentKPI][i], std::ios::in);

				if (file.is_open())
				{
					while (!file.eof())
					{
						// Read signal name
						getline(file, input, ',');

						// If current line contains MCP_TOTAL values
						if (input.find("P_MCP_TOTAL") != std::string::npos)
						{
							// Ignore peak
							getline(file, input, ',');

							// Read and store average
							getline(file, input, ',');

							g_averageMap[i] = std::stof(input);

							break;
						}

						// If not, ignore the rest of the line
						else
							getline(file, input);
					}

					file.close();
				}

				else
				{
					std::cout << "Failed to open file when determining three tightest.\n";

					return false;
				}
			}

			// Store all possible combinations of three averages
			Push3(0, 3);

			// Find combination with lowest distances between values
			for (int i = 0; i < g_vCombinations.size(); ++i)
			{
				currentDistance = 0;

				currentDistance += abs(g_vCombinations[i][0] - g_vCombinations[i][1]);
				currentDistance += abs(g_vCombinations[i][1] - g_vCombinations[i][2]);
				currentDistance += abs(g_vCombinations[i][2] - g_vCombinations[i][0]);

				currentDistance /= 3;

				if (currentDistance < bestDifference)
				{
					bestDifference = currentDistance;

					comboIndex = i;
				}
			}

			// Remove file paths of result files containing MCP_TOTAL values out of nearest-three grouping, leaving three file paths in vector
			for (int i = 0; i < g_vFilePaths[nCurrentKPI].size(); ++i)
			{
				if (std::find(g_vCombinations[comboIndex].begin(), g_vCombinations[comboIndex].end(), g_averageMap[i]) != g_vCombinations[comboIndex].end())
					newPaths.push_back(g_vFilePaths[nCurrentKPI][i]);
			}

			g_vFilePaths[nCurrentKPI] = newPaths;
		}
	}

	return true;
}

void Push3(int a_nOffset, int a_nSize)
{
	if (!a_nSize)
	{
		g_vCombinations.push_back(g_vGroup);

		return;
	}

	for (int i = a_nOffset; i <= g_averageMap.size() - a_nSize; ++i)
	{
		g_vGroup.push_back(g_averageMap[i]);

		Push3(i + 1, a_nSize - 1);

		g_vGroup.pop_back();
	}
}

bool StoreData()
{
	Average avgInstance;

	std::ifstream inFile;

	std::string input;

	std::stringstream ss;

	std::vector<std::string> strVec;

	std::vector<Average> avgVec;

	float fValue;

	int nCurrentLine = 0, nNumRuns;

	// Do three runs max
	nNumRuns = (g_vFilePaths[g_nCurrentKPI].size() < 3 ? g_vFilePaths[g_nCurrentKPI].size() : 3);

	for (int nCurrentRun = 0; nCurrentRun < nNumRuns; ++nCurrentRun)
	{
		inFile.open(g_vFilePaths[g_nCurrentKPI][nCurrentRun], std::ios::in);

		if (inFile.is_open())
		{
			// Read header
			std::getline(inFile, input);

			while (!inFile.eof())
			{
				// Read signal name
				std::getline(inFile, input, ',');

				// If first pass, store it and push average object
				if (nCurrentRun == 0)
				{
					g_vSignals.push_back(strVec);

					g_vSignals[g_nCurrentKPI].push_back(input);

					g_vAverages.push_back(avgVec);

					g_vAverages[g_nCurrentKPI].push_back(avgInstance);
				}

				// Ignore peak
				std::getline(inFile, input, ',');

				// Read and store average
				std::getline(inFile, input, ',');

				ss.clear();
				ss.str("");

				if (input == "")
					ss << "-999";

				else
					ss << input;

				ss >> g_vAverages[g_nCurrentKPI][nCurrentLine].aValue[nCurrentRun];

				// Ignore peak time
				std::getline(inFile, input);

				++nCurrentLine;
			}

			nCurrentLine = 0;

			inFile.close();
		}

		else
		{
			std::cout << "Failed to open summary file for reading:\n" << g_vFilePaths[g_nCurrentKPI][nCurrentRun - 1] << "\n\n";

			return false;
		}
	}

	return true;
}

bool WriteData()
{
	if (!g_bAuto)
		g_xWorkingDirectory = GetPath("Select output file location...");

	std::ofstream file(g_xWorkingDirectory + "\\Combined Summary.csv", std::ios::out);

	std::vector<int>::iterator it;

	int index = 0;

	if (!file.is_open())
		return false;

	// Write header lines
	file << ",modern_connected_standby_wlan,,,,idle_display_on_psr,,,,ICOB,,,,video_playback_tos_1080p24_h264_42,,,,video_playback_tos_2160p24_h265_420_10b,,,,HOBL Netflix,,,,Teams 1x1,,,,MM25\n";

	file << "Signal Name,Run_1,Run_2,Run_3,CS,Run_1,Run_2,Run_3,Idle,Run_1,Run_2,Run_3,icob,Run_1,Run_2,Run_3,1080p,Run_1,Run_2,Run_3,2160p,Run_1,Run_2,Run_3,Netflix,Run_1,Run_2,Run_3,Teams,Run_1,Run_2,Run_3,mm25\n";

	for (int i = 0; i < g_vSignals[0].size(); ++i)
	{
		// Signal name
		file << g_vSignals[0][i] << ",";

		for (int j = 0; j < 8; ++j)
		{
			it = std::find(g_vKPIIndices.begin(), g_vKPIIndices.end(), j);

			index = std::distance(g_vKPIIndices.begin(), it);

			if (index < g_vKPIIndices.size())
			{
				file << (g_vAverages[index][i].aValue[0] != -999 ? std::to_string(g_vAverages[index][i].aValue[0]) : "") << ","		// Average 1
					 << (g_vAverages[index][i].aValue[1] != -999 ? std::to_string(g_vAverages[index][i].aValue[1]) : "") << ","		// Average 2
					 << (g_vAverages[index][i].aValue[2] != -999 ? std::to_string(g_vAverages[index][i].aValue[2]) : "") << ",";	// Average 3

				// Median
				std::sort(g_vAverages[index][i].aValue, g_vAverages[index][i].aValue + sizeof(g_vAverages[index][i].aValue) / sizeof(float));

				file << (g_vAverages[index][i].aValue[1] != -999 ? std::to_string(g_vAverages[index][i].aValue[1]) : "") << ",";
			}

			else
			{
				file << ",,,,";
			}
		}

		file << "\n";
	}

	file.close();

	return true;
}
