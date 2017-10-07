#include "Tracker/defines.h"
#include "./deepsort/tracker.h"
#include "StrCommon.h"
#include "Tracker/ds/FeatureGetter.h"
#include "deepsort/tracker.h"

NearestNeighborDistanceMetric *NearestNeighborDistanceMetric::self_ = NULL;
FeatureGetter *FeatureGetter::self_ = NULL;
KF *KF::self_ = NULL;


TTracker *_tt = NULL;
void DrawTrack(cv::Mat frame,
                   const KalmanTracker& track
                   ){
	if (!track.is_confirmed() || track.time_since_update_ > 0) {
		return;
	}
	DSBOX box = track.to_tlwh();
	cv::Rect rc;
	rc.x = box(0);
	rc.y = box(1);
	rc.width = box(2);
	rc.height = box(3);
	CvScalar clr = cvScalar(0, 255, 0);
    cv::rectangle(frame, rc, clr);
	std::string disp = toStr((int)track.track_id);
	cv::putText(frame, 
		disp, 
		cvPoint(rc.x, rc.y), 
		CV_FONT_HERSHEY_SIMPLEX, 
		0.6, 
		cv::Scalar(0, 0, 255));
	
}
void DrawData(cv::Mat frame){
	std::vector<KalmanTracker*> &kalmanTrackers =
		_tt->kalmanTrackers_;

    for (const auto& track : kalmanTrackers){
        DrawTrack(frame, *track);
    }
}


void ReadFileContent(const std::string &file, std::string &content){
	FILE *fl = fopen(file.c_str(), "rb");
	if(fl == NULL){
		return;
	}
	fseek(fl, 0, SEEK_END);
	int len = ftell(fl);
	if(len <= 0){
		return;
	}
	fseek(fl, 0, SEEK_SET);
	char *buf = new char[len+1];
	memset(buf, 0, len+1);
	fread(buf, 1, len, fl);
	content = std::string(buf);
	delete []buf;
	fclose(fl);
}

std::map<int, std::vector<cv::Rect>> _rcMap;
void ReadRcFileTotal(const std::string &file) {
	std::string content = "";
	ReadFileContent(file, content);


	std::vector<std::string> lines;
	splitStr(content, "\n", lines);
	std::vector<cv::Rect> rcs;
	int num = -1;
	int tmpNum = -1;
	for (int i = 0; i < lines.size(); i++) {
		std::vector<std::string> cols;
		splitStr(lines[i], ",", cols);
		if (cols.size() < 6) {
			continue;
		}
		tmpNum = toInt(trim(cols[0]));
		if (num!=-1 && tmpNum!=num) {
			_rcMap.insert(std::make_pair(num, rcs));
			rcs.clear();
			num = tmpNum;
		}
		if (num == -1) {
			num = tmpNum;
		}
		cv::Rect rc;
		rc.x = toInt(trim(cols[2]));
		rc.y = toInt(trim(cols[3]));
		rc.width = toInt(trim(cols[4]));
		rc.height = toInt(trim(cols[5]));
		rcs.push_back(rc);
	}
	if (!rcs.empty()) {
		_rcMap.insert(std::make_pair(tmpNum, rcs));
	}
}
std::string _rcFile = "";
std::string _imgDir;
VideoWriter *_vw = NULL;
bool _isShow = false;
int _imgCount = 0;
std::vector<cv::Rect> _lastRcs;

void CB(Mat &frame, int num){
	if (_vw == NULL) {
		_vw = new VideoWriter("out.avi", CV_FOURCC('M', 'J', 'P', 'G'), 25.0, Size(frame.cols, frame.rows));
	}

	if (_rcMap.empty()) {
		ReadRcFileTotal(_rcFile);
	}
	std::vector<cv::Rect> rcs;
	std::map<int, std::vector<cv::Rect>>::iterator it = _rcMap.find(num);
	if (it != _rcMap.end()) {
		rcs = it->second;
	}
	Mat mm;
	{
		mm = frame.clone();
		for(int i = 0; i < rcs.size(); i++){
			cv::Rect rc = rcs[i];
			cv::rectangle(mm, rc, cvScalar(0, 255, 0));
		}
	}


	std::vector<Detection> dets;
	std::vector<FEATURE> fts;
	FeatureGetter::Instance()->Get(frame, rcs, fts);

	for (int i = 0; i < rcs.size(); i++){	
		DSBOX box;
		cv::Rect rc = rcs[i];
		box(0) = rc.x;
		box(1) = rc.y;
		box(2) = rc.width;
		box(3) = rc.height;
		Detection det(box, 1, fts[i]);
		dets.push_back(det);
	}
	if (num == 117) {
		std::cout << "117\n";
	}
    _tt->update(dets);
	DrawData(frame);
	//(*_vw) << frame;
	if(_isShow){
		std::string disp = "frame";
		resize(mm, mm, Size(mm.cols/2, mm.rows/2));
		resize(frame, frame, Size(frame.cols/2, frame.rows/2));
		imshow("mm", mm);
		imshow(disp, frame);
		waitKey(1);
	}
}

void Go() {
	std::string root = _imgDir;
	for (int i = 1; i < _imgCount; i++) {
		std::string path = root;
		path += to6dStr(i);
		path += ".jpg";
		cv::Mat mat = imread(path);
		CB(mat, i);
		printf("finish %d frame\n", i);
	}

}

int main(int argc, char **argv){
	if(0){
		Eigen::Matrix<float, -1, 4> test;
		Eigen::Matrix<float, 1, 4, Eigen::RowMajor> row;
		row(0, 0) = 1;
		row(0, 1) = 2;
		row(0, 2) = 3;
		row(0, 3) = 4;
		test.resize(test.rows() + 1, 4);
		test.row(0) = row;
		std::cout << test << std::endl << "-----------" << std::endl;
		test.resize(test.rows()+1, 4);
		test.row(1) = row;
		std::cout << test << std::endl << "-----------" << std::endl;
		test.resize(test.rows() + 1, 4);
		test.row(2) = row;
		std::cout << test << std::endl << "-----------" << std::endl;
	}
	if (argc < 2) {
		printf("usage:\n./tt showornot(0/1)\n");
		return 0;
	}
	_isShow = toInt(argv[1]);
	FeatureGetter::Instance()->Init();
	KF::Instance()->Init();
	_tt = new TTracker();
	NearestNeighborDistanceMetric::Instance()->Init(0.2, 100);

	//_imgDir = "e:/code/deep_sort-master/MOT16/ff/fr/img1/";
	//_rcFile = "e:/code/deep_sort-master/MOT16/ff/fr/det/det.txt";
	_imgDir = "e:/code/deep_sort-master/MOT16/tt/xyz2/img1/";
	_rcFile = "e:/code/deep_sort-master/MOT16/tt/xyz2/det/det.txt";
	_imgCount = 750;// 2001;// 750;// 680;
	Go();
	return 0;
}
