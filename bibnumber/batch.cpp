#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/multiset_of.hpp>

#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "batch.h"
#include "pipeline.h"
#include "debug.h"

namespace bimaps = boost::bimaps;
namespace fs = boost::filesystem;

class CSVRow {
public:
	std::string const& operator[](std::size_t index) const {
		return m_data[index];
	}
	std::size_t size() const {
		return m_data.size();
	}
	void readNextRow(std::istream& str) {
		std::string line;
		std::getline(str, line);

		std::stringstream lineStream(line);
		std::string cell;

		m_data.clear();
		while (std::getline(lineStream, cell, ';')) {
			m_data.push_back(cell);
		}
	}
private:
	std::vector<std::string> m_data;
};

std::istream& operator>>(std::istream& str, CSVRow& data) {
	data.readNextRow(str);
	return str;
}

static bool isImageFile(std::string name) {
	std::string lower_case(name);
	std::transform(lower_case.begin(), lower_case.end(), lower_case.begin(),
			::tolower);

	if ((boost::algorithm::ends_with(lower_case, ".jpg"))
			|| (boost::algorithm::ends_with(lower_case, ".png")))
		return true;
	else
		return false;

}

#if 0
int batch(const char *path) {
	if (boost::algorithm::ends_with(path, ".csv")) {
		std::ifstream file(path);

		CSVRow row;
		while (file >> row) {
			std::cout << "4th Element(" << row[3] << ")\n";
		}
	}

	return 0;
}
#endif

static int processSingleImage(std::string fileName,
		std::vector<int>& bibNumbers) {
	int res;

	std::cout << "Processing file " << fileName << std::endl;

	/* open image */
	cv::Mat image = cv::imread(fileName, 1);
	if (image.empty()) {
		std::cerr << "ERROR:Failed to open image file" << std::endl;
		return -1;
	}

	/* process image */
	res = pipeline::processImage(image, bibNumbers);
	if (res < 0) {
		std::cerr << "ERROR: Could not process image" << std::endl;
		return -1;
	}

	/* remove duplicates */
	std::sort(bibNumbers.begin(), bibNumbers.end());
	bibNumbers.erase(std::unique(bibNumbers.begin(), bibNumbers.end()),
			bibNumbers.end());

	/* display result */
	std::cout << "Read: [";
	for (std::vector<int>::iterator it = bibNumbers.begin();
			it != bibNumbers.end(); ++it) {
		std::cout << " " << *it;
	}
	std::cout << "]" << std::endl;

	return res;
}

static int exists(std::vector<int> arr, int item) {
	return std::find(arr.begin(), arr.end(), item) != arr.end();
}

namespace batch {

int process(std::string inputName) {
	int res;

	std::string resultFileName("out.csv");

	if (!fs::exists(inputName)) {
		std::cerr << "ERROR: Not found: " << inputName << std::endl;
		return -1;
	}

	if (fs::is_regular_file(inputName)) {
		/* convert name to lower case to make extension checks easier */
		std::string name(inputName);
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		if (isImageFile(inputName)) {
			std::vector<int> bibNumbers;
			res = processSingleImage(inputName, bibNumbers);
		} else if (boost::algorithm::ends_with(name, ".csv")) {

			int true_positives = 0;
			int false_positives = 0;
			int relevant = 0;

			/* set debug mask to minimum */
			debug::set_debug_mask(DBG_NONE);

			std::ifstream file(inputName.c_str());
			fs::path pathname(inputName);
			fs::path dirname = pathname.parent_path();

			CSVRow row;
			while (file >> row) {
				std::string filename = row[0];
				std::vector<int> groundTruthNumbers;
				std::vector<int> bibNumbers;

				fs::path file(filename);
				fs::path full_path = dirname / file;

				processSingleImage(full_path.string(), bibNumbers);

				for (unsigned int i = 1; i < row.size(); i++)
					groundTruthNumbers.push_back(atoi(row[i].c_str()));
				relevant += groundTruthNumbers.size();

				for (unsigned int i = 0; i < bibNumbers.size(); i++) {
					if (exists(groundTruthNumbers, bibNumbers[i])) {
						std::cout << "Match " << bibNumbers[i] << std::endl;
						true_positives++;
					} else {
						std::cout << "Mismatch " << bibNumbers[i] << std::endl;
						false_positives++;
					}
				}

				for (unsigned int i = 0; i < groundTruthNumbers.size(); i++) {
					if (!exists(bibNumbers, groundTruthNumbers[i])) {
						std::cout << "Missed " << groundTruthNumbers[i]
								<< std::endl;
					}
				}

			}

			std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);
			std::cout.precision(2);

			float precision = (float) true_positives
					/ (float) (true_positives + false_positives);
			float recall = (float) true_positives / (float) (relevant);
			float fscore = 2 * precision * recall / (precision + recall);

			std::cout << "precision=" << true_positives << "/"
					<< true_positives + false_positives << "=" << precision
					<< std::endl;
			std::cout << "recall=" << true_positives << "/" << relevant << "="
					<< recall << std::endl;
			std::cout << "F-score=" << fscore << std::endl;

		}
	} else if (fs::is_directory(inputName)) {

		fs::path outPath = inputName / fs::path(resultFileName);
		std::cout << "Processing directory " << inputName << " into "
				<< outPath.string() << std::endl;

		std::ofstream outFile;
		outFile.open(outPath.c_str());

		/* set debug mask to minimum */
		debug::set_debug_mask(DBG_NONE);

		typedef std::vector<fs::path> vec;             // store paths,
		vec v;// so we can sort them later

		std::copy(fs::directory_iterator(inputName), fs::directory_iterator(),
				back_inserter(v));

		sort(v.begin(), v.end());// sort, since directory iteration
									// is not ordered on some file systems

									//typedef bm::bimap<bm::multiset_of<std::string>,
									//		bm::multiset_of<long, std::greater<long> > > imgTagBimap;

		typedef boost::bimap<bimaps::multiset_of<std::string>,
						bimaps::multiset_of<int> > imgTagBimap;

		imgTagBimap tags;

		for (vec::const_iterator it(v.begin()); it != v.end(); ++it) {
			if (isImageFile(it->string())) {
				std::vector<int> bibNumbers;
				res = processSingleImage(it->string(), bibNumbers);

				for (unsigned int i = 0; i < bibNumbers.size(); i++) {
					tags.insert(imgTagBimap::value_type(it->string(),bibNumbers[i]) );
				}
			}
		}

		std::cout << "Saving results to " << outPath.string() << std::endl;

		int current_bib = 0;
		for (imgTagBimap::right_const_iterator it=tags.right.begin(), iend=tags.right.end();
				it!=iend; it++) {
			if (it->first != current_bib) {
				current_bib = it->first;
				outFile << std::endl << current_bib << ",";
			}
			outFile << it->second << ",";
		}

		outFile.close();

		return -1;
	} else {
		std::cerr << "ERROR: unknown path type " << inputName << std::endl;
		return -1;
	}

	return res;
}

} /* namespace batch */

