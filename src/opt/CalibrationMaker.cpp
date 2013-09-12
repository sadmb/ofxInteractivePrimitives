#include "CalibrationMaker.h"

#include "Poco/Util/XMLConfiguration.h"

#include "ofxCv.h"

static ofMatrix4x4 homography2glModelViewMatrix(const cv::Mat &homography);

#pragma mark - CalibrationMaker

void CalibrationMaker::draw()
{
	Marker::draw();
	
	if (hasFocus())
	{
		ofNoFill();
		ofRect(-15, -15, 30, 30);
	}
}

void CalibrationMaker::update()
{
	stringstream ss;
	ss << id << endl;
	ss << getX() << ":" << getY() << endl;
	ss << (int)object_pos.x << ":" << (int)object_pos.y << ":" << (int)object_pos.z;
	
	text = ss.str();
}

void CalibrationMaker::keyPressed(int key)
{
	if (key == OF_KEY_LEFT)
		move(-1, 0, 0);
	else if (key == OF_KEY_RIGHT)
		move(1, 0, 0);
	else if (key == OF_KEY_UP)
		move(0, -1, 0);
	else if (key == OF_KEY_DOWN)
		move(0, 1, 0);
}

#pragma mark - Manager

ofMatrix4x4 CalibrationMaker::Manager::getHomography()
{
	using namespace cv;
	using namespace ofxCv;
	
	vector<Point2f> srcPoints, dstPoints;
	for(int i = 0; i < markers.size(); i++)
	{
		CalibrationMaker::Ref o = markers[i];
		
		dstPoints.push_back(Point2f(o->getX(), o->getY()));
		srcPoints.push_back(Point2f(o->object_pos.x, o->object_pos.y));
	}
	
	Mat homography = findHomography(Mat(srcPoints), Mat(dstPoints));
	return homography2glModelViewMatrix(homography);
}

static ofMatrix4x4 cameraMatrixToProjectionMatrix(cv::Mat cameraMatrix, float w, float h, float n, float f)
{
	double fx = cameraMatrix.at<double>(0, 0);
	double fy = cameraMatrix.at<double>(1, 1);
	double cx = cameraMatrix.at<double>(0, 2);
	double cy = cameraMatrix.at<double>(1, 2);
	
	float ww = 2 / w;
	float hh = 2 / h;

	return ofMatrix4x4
	(ww * fx, 0.0, ww * cx - 1, 0,
	 0, -hh * fy, -hh * cy - 1, 0,
	 0, 0, (-f + n) / (f - n), (-2 * f * n) / (f - n),
	 0, 0, -1, 0
	 );
//	return ofMatrix4x4
//	(2 * fx / w, 0, (w - 2 * cx) / w, 0,
//	 0, -2 * fy / h, (h - 2 * cy) / h, 0,
//	 0, 0, (-f - n) / (f - n), -2 * f * n / (f - n),
//	 0, 0, -1, 0
//	 );
}

pair<ofMatrix4x4, ofMatrix4x4> CalibrationMaker::Manager::getEstimatedCameraPose(int width, int height, float fov)
{
	assert(markers.size() >= 6);
	
	using namespace ofxCv;
	using namespace cv;
	
	Size2i imageSize(width, height);
	float fy = imageSize.height * ofDegToRad(fov);
	float fx = fy * (float)imageSize.width / imageSize.height;
	Point2f c = Point2f(imageSize) * (1. / 2);
	Mat1d cameraMatrix = (Mat1d(3, 3) <<
						  fx, 0, c.x,
						  0, fy, c.y,
						  0, 0, 1);
	
	int flags = CV_CALIB_USE_INTRINSIC_GUESS | CV_CALIB_FIX_ASPECT_RATIO;
	
//	flags |= CV_CALIB_ZERO_TANGENT_DIST;
//	flags |= CV_CALIB_FIX_PRINCIPAL_POINT;
	
	flags |= (CV_CALIB_FIX_K1 | CV_CALIB_FIX_K2 | CV_CALIB_FIX_K3 | CV_CALIB_FIX_K4 | CV_CALIB_FIX_K5 | CV_CALIB_FIX_K6 | CV_CALIB_RATIONAL_MODEL);
	
	vector<Mat> rvecs, tvecs;
	Mat distCoeffs;
	
	vector< vector<Point3f> > objectPoints(1);
	vector< vector<Point2f> > imagePoints(1);
	
	for(int i = 0; i < markers.size(); i++)
	{
		CalibrationMaker::Ref o = markers[i];
		objectPoints[0].push_back(toCv(o->object_pos));
		imagePoints[0].push_back(toCv((ofVec2f)o->getPosition()));
	}
	
	cv::Mat rvec, tvec;
	
	calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, flags);
	rvec = rvecs[0];
	tvec = tvecs[0];
	
	ofMatrix4x4 m = makeMatrix(rvec, tvec);
	m.postMultScale(ofVec3f(1, -1, -1));
	
	cout << cameraMatrixToProjectionMatrix(cameraMatrix, width, height, 1, 10000) << endl;
	
	
	
	return make_pair<ofMatrix4x4, ofMatrix4x4>(m.getInverse(), cameraMatrixToProjectionMatrix(cameraMatrix, width, height, 1, 10000));
}

void CalibrationMaker::Manager::setup(int num_markers)
{
	for (int i = 0; i < num_markers; i++)
	{
		markers.push_back(CalibrationMaker::Ref(new CalibrationMaker(i, root)));
	}
}

void CalibrationMaker::Manager::update()
{
	root.update();
}

void CalibrationMaker::Manager::draw()
{
	ofPushStyle();
	
	if (root.getFocusObject())
	{
		ofPushStyle();
		
		ofSetLineWidth(4);
		
		ofSetColor(255, 0, 0);
		
		ofVec2f p = root.getFocusObject()->getPosition();
		ofNoFill();
		ofCircle(p, 40);
		
		ofFill();
		ofCircle(p, 10);
		
		ofLine(-10000, p.y, 10000, p.y);
		ofLine(p.x, -10000, p.x, 10000);
		
		ofPopStyle();
	}

	root.draw();
	
	ofPopStyle();
}

void CalibrationMaker::Manager::load(string path)
{
	if (!ofFile::doesFileExist(path)) return;
	
	Poco::AutoPtr<Poco::Util::XMLConfiguration> config = new Poco::Util::XMLConfiguration;
	config->loadEmpty("markers");
	config->load(ofToDataPath(path));
	
	for (int i = 0; i < markers.size(); i++)
	{
		CalibrationMaker::Ref o = markers[i];
		
		string m = "marker[" + ofToString(i) + "]";
		
		o->setPosition(config->getDouble(m + ".image[@x]", 0),
					   config->getDouble(m + ".image[@y]", 0),
					   0);
		
		o->object_pos.set(config->getDouble(m + ".object[@x]", 0),
						  config->getDouble(m + ".object[@y]", 0),
						  config->getDouble(m + ".object[@z]", 0));
	}
	
}

void CalibrationMaker::Manager::save(string path)
{
	Poco::AutoPtr<Poco::Util::XMLConfiguration> config = new Poco::Util::XMLConfiguration;
	config->loadEmpty("markers");
	
	for (int i = 0; i < markers.size(); i++)
	{
		CalibrationMaker::Ref o = markers[i];
		
		string m = "marker[" + ofToString(i) + "]";
		
		config->setDouble(m + ".image[@x]", o->getX());
		config->setDouble(m + ".image[@y]", o->getY());
		
		config->setDouble(m + ".object[@x]", o->object_pos.x);
		config->setDouble(m + ".object[@y]", o->object_pos.y);
		config->setDouble(m + ".object[@z]", o->object_pos.z);
	}
	
	config->save(ofToDataPath(path));
}

void CalibrationMaker::Manager::setImagePoint(int x, int y, int z)
{
	CalibrationMaker *m = (CalibrationMaker*)root.getFocusObject();
	if (m) m->object_pos.set(x, y, z);
}

static ofMatrix4x4 homography2glModelViewMatrix(const cv::Mat &homography)
{
	ofMatrix4x4 matrix;
	
	matrix(0, 0) = homography.at<double>(0, 0);
	matrix(0, 1) = homography.at<double>(1, 0);
	matrix(0, 2) = 0;
	matrix(0, 3) = homography.at<double>(2, 0);
	
	matrix(1, 0) = homography.at<double>(0, 1);
	matrix(1, 1) = homography.at<double>(1, 1);
	matrix(1, 2) = 0;
	matrix(1, 3) = homography.at<double>(2, 1);
	
	matrix(2, 0) = 0;
	matrix(2, 1) = 0;
	matrix(2, 2) = 1;
	matrix(2, 3) = 0;
	
	matrix(3, 0) = homography.at<double>(0, 2);
	matrix(3, 1) = homography.at<double>(1, 2);
	matrix(3, 2) = 0;
	matrix(3, 3) = 1;
	
	return matrix;
}

