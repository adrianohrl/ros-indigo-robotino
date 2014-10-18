/*
 * RobotinoVision.cpp
 *
 *  Created on: 15/07/2014
 *      Author: adrianohrl@unifei.edu.br
 */

#include "RobotinoVision.h"

RobotinoVision::RobotinoVision()
	: it_(nh_)
{
	nh_.param<double>("camera_height", camera_height_, 28.0); // in centimeters
	nh_.param<double>("camera_close_distance", camera_close_distance_, 28.0); // in centimeters
	nh_.param<double>("camera_far_distance", camera_far_distance_, 115.0); // in centimeters
	nh_.param<double>("camera_depth_width", camera_depth_width_, 88.0); // in centimeters
	nh_.param<int>("height", height_, 240); // in pixels
	nh_.param<int>("width", width_, 320); // in pixels

	find_objects_srv_ = nh_.advertiseService("find_objects", &RobotinoVision::findObjects, this);
	image_sub_ = it_.subscribe("image_raw", 1, &RobotinoVision::imageCallback, this);
	save_srv_ = nh_.advertiseService("save_image", &RobotinoVision::saveImage, this);
	set_calibration_srv_ = nh_.advertiseService("set_calibration", &RobotinoVision::setCalibration, this);

	imgRGB_ = Mat(width_, height_, CV_8UC3, Scalar::all(0));

	setColor(RED);	

	namedWindow(BLACK_MASK_WINDOW);
	namedWindow(PUCKS_MASK_WINDOW);
	namedWindow(COLOR_MASK_WINDOW);
	namedWindow(FINAL_MASK_WINDOW);
	namedWindow(BGR_WINDOW);
	namedWindow(CONTOURS_WINDOW);
	moveWindow(BLACK_MASK_WINDOW, 0 * width_, 600);
	moveWindow(PUCKS_MASK_WINDOW, 1 * width_, 600);
	moveWindow(COLOR_MASK_WINDOW, 2 * width_, 600);
	moveWindow(FINAL_MASK_WINDOW, 3 * width_, 600);
	moveWindow(BGR_WINDOW, 4 * width_, 600);
	moveWindow(CONTOURS_WINDOW, 5 * width_, 600);
	calibration_ = true;
}

RobotinoVision::~RobotinoVision()
{
	find_objects_srv_.shutdown();
	image_sub_.shutdown();
	save_srv_.shutdown();
	set_calibration_srv_.shutdown();
	destroyAllWindows();
}

bool RobotinoVision::spin()
{
	ROS_INFO("Robotino Vision Node up and running!!!");
	ros::Rate loop_rate(30);
	while(nh_.ok())
	{
		ros::spinOnce();
		loop_rate.sleep();
		ROS_INFO("Processing Color!!!");
		vector<Point2f> mass_center = processColor();
		ROS_INFO("Getting Positions!!!");
		//vector<Point2f> positions = getPositions(mass_center);
		/*const char* imageName = "/home/adriano/catkin_ws/src/robotino/robotino_vision/samples/pucks.jpg";
		Mat imgBGR = readImage(imageName);
		cvtColor(imgBGR, imgRGB_, CV_BGR2RGB);*/
	}
	return true;
}

bool RobotinoVision::saveImage(robotino_vision::SaveImage::Request &req, robotino_vision::SaveImage::Response &res)
{
	imwrite(req.image_name.c_str(), imgRGB_);
	return true;
}

bool RobotinoVision::setCalibration(robotino_vision::SetCalibration::Request &req, robotino_vision::SetCalibration::Response &res)
{
	if (req.calibration)
	{
		namedWindow(BLACK_MASK_WINDOW);
		namedWindow(PUCKS_MASK_WINDOW);
		namedWindow(COLOR_MASK_WINDOW);
		namedWindow(FINAL_MASK_WINDOW);
		namedWindow(BGR_WINDOW);
		namedWindow(CONTOURS_WINDOW);
		moveWindow(BLACK_MASK_WINDOW, 0 * width_, 600);
		moveWindow(PUCKS_MASK_WINDOW, 1 * width_, 600);
		moveWindow(COLOR_MASK_WINDOW, 2 * width_, 600);
		moveWindow(FINAL_MASK_WINDOW, 3 * width_, 600);
		moveWindow(BGR_WINDOW, 4 * width_, 600);
		moveWindow(CONTOURS_WINDOW, 5 * width_, 600);
	}
	else 
	{
		destroyAllWindows();
	}
	calibration_ = req.calibration;
	return true;
}

bool RobotinoVision::findObjects(robotino_vision::FindObjects::Request &req, robotino_vision::FindObjects::Response &res)
{
	switch (req.color)
	{
		case 0:
			setColor(RED);
			break;
		case 1:
			setColor(GREEN);
			break;
		case 2:
			setColor(BLUE);
			break;
		case 3:
			setColor(YELLOW);
	}
	vector<Point2f> mass_center = processColor();
	vector<Point2f> positions = getPositions(mass_center);
	vector<float> distances(positions.size());
	vector<float> directions(positions.size());
	for (int k = 0; k < positions.size(); k++)
	{
		distances[k] = positions[k].x;
		directions[k] = positions[k].y;
	}
	res.distances = distances;
	res.directions = directions;
	return true;
}

void RobotinoVision::imageCallback(const sensor_msgs::ImageConstPtr& msg)
{
	cv_bridge::CvImagePtr cv_ptr;
	try
	{
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::RGB8);
	}
	catch (cv_bridge::Exception& e)
	{
		ROS_ERROR("cv_bridge exception: %s", e.what());
		return;
	} 
	imgRGB_ = cv_ptr->image;
}

cv::Mat RobotinoVision::readImage(const char* imageName)
{
	Mat image;
	image = imread(imageName, 1);
	
	if (!image.data)
	{
		ROS_ERROR("No image data!!!");
		return Mat(width_, height_, CV_8UC3, Scalar::all(0));
	}
	
	return image;
}

std::vector<cv::Point2f> RobotinoVision::processColor()
{
	ROS_INFO("Getting Black Mask!!!");
	Mat black_mask = getBlackMask();
	ROS_INFO("Getting Pucks Mask!!!");
	Mat pucks_mask = getPucksMask();
	ROS_INFO("Getting Color Mask!!!");
	Mat color_mask = getColorMask();
	ROS_INFO("Getting Final Mask!!!");
	Mat final_mask = getFinalMask(black_mask, pucks_mask, color_mask);
	ROS_INFO("Getting Contours based on Final Mask!!!");
	getContours(final_mask);
	
	if (calibration_)
	{
		createTrackbar("Value threshold parameter: ", BLACK_MASK_WINDOW, &thresh0_, 255);
		createTrackbar("Erosion size parameter: ", BLACK_MASK_WINDOW, &erosion0_, 20);
		imshow(BLACK_MASK_WINDOW, black_mask);
ROS_INFO("10");	
		createTrackbar("Value threshold parameter: ", PUCKS_MASK_WINDOW, &thresh1_, 255);
		createTrackbar("Close size parameter: ", PUCKS_MASK_WINDOW, &close1_, 20);
		createTrackbar("Open size parameter: ", PUCKS_MASK_WINDOW, &open1_, 20);
		imshow(PUCKS_MASK_WINDOW, pucks_mask);
	
		createTrackbar("Initial range value: ", COLOR_MASK_WINDOW, &initialRangeValue_, 255);
		createTrackbar("Range width: ", COLOR_MASK_WINDOW, &rangeWidth_, 255);
		imshow(COLOR_MASK_WINDOW, color_mask);
	
		createTrackbar("Open size parameter: ", FINAL_MASK_WINDOW, &open2_, 20);
		createTrackbar("Close size parameter: ", FINAL_MASK_WINDOW, &close2_, 20);
		imshow(FINAL_MASK_WINDOW, final_mask);
	
		//showImageBGRwithMask(final_mask);
	
		waitKey(3);
	}
	black_mask.release();
	pucks_mask.release();
	color_mask.release();
	final_mask.release();	
}

cv::Mat RobotinoVision::getBlackMask()
{
	Mat black_mask;

	// convertendo de RGB para HSV
	Mat imgHSV;
	cvtColor(imgRGB_, imgHSV, CV_RGB2HSV);

	// separando a HSV 
	Mat splitted[3];
	split(imgHSV, splitted);
	black_mask = splitted[2];

	// fazendo threshold da imagem V
	threshold(black_mask, black_mask, thresh0_, 255, THRESH_BINARY);

	// fazendo erosão na imagem acima
	Mat element = getStructuringElement(MORPH_RECT, Size(2 * erosion0_ + 1, 2 * erosion0_ + 1), Point(erosion0_, erosion0_));
	erode(black_mask, black_mask, element);

	imgHSV.release();
	splitted[0].release();
	splitted[1].release();
	splitted[2].release();
	element.release();

	return black_mask;
}

cv::Mat RobotinoVision::getPucksMask()
{
	Mat pucks_mask;
	
	// convertendo de RGB para HSV
	Mat imgHSV;
	cvtColor(imgRGB_, imgHSV, CV_RGB2HSV);

	// separando a HSV 
	Mat splitted[3];
	split(imgHSV, splitted);
	pucks_mask = splitted[1];
ROS_INFO("1");
	// fazendo threshold da imagem S
	threshold(pucks_mask, pucks_mask, thresh1_, 255, THRESH_BINARY);
ROS_INFO("2");	
	// fechando buracos
	Mat element = getStructuringElement(MORPH_CROSS, Size(2 * close1_ + 1, 2 * close1_ + 1), Point(close1_, close1_));
ROS_INFO(" Close = %d", close1_ );
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//Mat element = getStructuringElement(MORPH_CROSS, Size(2 * close1_ + 3 ,2 * close1_ + 3), Point(2 * close1_ + 1, 2 * close1_ + 1));
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


ROS_INFO("3");
	morphologyEx(pucks_mask, pucks_mask, 3, element);
ROS_INFO("4");
	// filtro de partícula pequenas
	element = getStructuringElement(MORPH_CROSS, Size(2 * open1_ + 1, 2 * open1_ + 1), Point(open1_, open1_));
	morphologyEx(pucks_mask, pucks_mask, 2, element);
ROS_INFO("5");
	imgHSV.release();
	splitted[0].release();
	splitted[1].release();
	splitted[2].release();
	element.release();

	return pucks_mask;
}

cv::Mat RobotinoVision::getColorMask()
{
	Mat color_mask;

	Mat imgBGR;
	cvtColor(imgRGB_, imgBGR, CV_RGB2BGR);
	imshow(BGR_WINDOW, imgBGR);

	// convertendo de RGB para HLS
	Mat imgHLS;
	cvtColor(imgRGB_, imgHLS, CV_RGB2HLS);

	// separando a HLS 
	Mat splitted[3];
	split(imgHLS, splitted);
	//double convertion_factor = 255 / 180;
	color_mask = 1.41666 * splitted[0];

	// rodando a roleta da imagem H
	Mat unsaturated = color_mask - initialRangeValue_;
	Mat saturated = color_mask + (255 - initialRangeValue_); 
	Mat aux;
	threshold(saturated, aux, 254, 255, THRESH_BINARY);
	aux = 255 - aux; 
	bitwise_and(saturated, aux, saturated);
	color_mask = unsaturated + saturated;
	if (calibration_)
	{
		imshow("Rotated HSL Model Window", color_mask);
	}

	// define o intervalo da cor
	threshold(color_mask, color_mask, rangeWidth_, 255, THRESH_BINARY);
	color_mask = 255 - color_mask;

	return color_mask;
}

cv::Mat RobotinoVision::getFinalMask(cv::Mat black_mask, cv::Mat pucks_mask, cv::Mat color_mask)
{
	// juntando todas as máscaras
	Mat final_mask = pucks_mask;
	bitwise_and(final_mask, black_mask, final_mask);
	bitwise_and(final_mask, color_mask, final_mask);

	// removendo particulas pequenas
	Mat element = getStructuringElement(MORPH_RECT, Size(2 * open2_ + 1, 2 * open2_ + 1), Point(open2_, open2_));
	morphologyEx(final_mask, final_mask, 2, element);
	
	// fechando buracos
	element = getStructuringElement(MORPH_RECT, Size(2 * close2_ + 1, 2 * close2_ + 1), Point(close2_, close2_));
	morphologyEx(final_mask, final_mask, 3, element);

	return final_mask;
}

void RobotinoVision::showImageBGRwithMask(cv::Mat mask)
{
	Mat imgBGR;
	cvtColor(imgRGB_, imgBGR, CV_RGB2BGR);
	imshow(BGR_WINDOW, imgBGR);

	Mat splitted[3];
	split(imgBGR, splitted);
	
	vector<Mat> channels;
	for (int i = 0; i < 3; i++)
	{
		bitwise_and(splitted[i], mask, splitted[i]);
		channels.push_back(splitted[i]);
	}
	merge(channels, imgBGR);
}

std::vector<cv::Point2f> RobotinoVision::getContours(cv::Mat input)
{
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;

	/// Find contours
	findContours(input, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

	/// Get the moments
	vector<Moments> mu(contours.size());
	for(int i = 0; i < contours.size(); i++)
	{
		mu[i] = moments(contours[i], false); 
	}

	///  Get the mass centers:
	vector<Point2f> mass_center(contours.size());
	for(int i = 0; i < contours.size(); i++)
	{
		mass_center[i] = Point2f(mu[i].m10/mu[i].m00, mu[i].m01/mu[i].m00); 
	}
	
	if (calibration_)
	{
		
		RNG rng(12345);
		ROS_INFO("******************************************");
		/// Draw contours
		Mat drawing = Mat::zeros(input.size(), CV_8UC3);
		for(int i = 0; i< contours.size(); i++)
		{
			Scalar color = Scalar(rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255));
			drawContours(drawing, contours, i, color, 2, 8, hierarchy, 0, Point());
			circle(drawing, mass_center[i], 4, color, -1, 8, 0);
			ROS_INFO("P%d = (xc, yc) = (%f, %f)", i, mass_center[i].x, mass_center[i].y);
		}

		/// Show in a window
		imshow(CONTOURS_WINDOW, drawing);
	}

	return mass_center;
}

std::vector<cv::Point2f> RobotinoVision::getPositions(std::vector<cv::Point2f> mass_center)
{
ROS_INFO("6");
	double alpha = atan(camera_close_distance_ / camera_height_);
ROS_INFO("7");
	double beta = atan(camera_far_distance_ / camera_height_);
ROS_INFO("8");
	vector<Point2f> positions(mass_center.size());
ROS_INFO("9");

	for (int k = 0; k < positions.size(); k++)
	{
		double i = mass_center[k].x;
		double j = mass_center[k].y;
		double theta = j * (beta - alpha) / height_ + beta;
		double distance = camera_height_ * tan(theta);
		double gama = atan(.5 * camera_depth_width_ / camera_close_distance_);
		double direction =  gama * (i - width_ / 2) / (width_ / 2);
		positions[k] = Point2f(distance, direction);
		ROS_INFO("(%d): Distance = %f, Direction = %f", k, distance, direction);
	}
	return positions;
	
}

void RobotinoVision::setColor(Color color)
{
	switch (color)
	{
		case RED:
			thresh0_ = 50;
			erosion0_ = 1;
			thresh1_ = 60;
			close1_ = 4;
			open1_ = 4;
			initialRangeValue_ = 90;
			rangeWidth_ = 32;
			open2_ = 8;
			close2_ = 4;
			break;
		case GREEN:
			thresh0_ = 50;
			erosion0_ = 1;
			thresh1_ = 60;
			close1_ = 4;
			open1_ = 4;
			initialRangeValue_ = 90;
			rangeWidth_ = 32;
			open2_ = 8;
			close2_ = 4;
			break;
		case BLUE:
			thresh0_ = 50;
			erosion0_ = 1;
			thresh1_ = 60;
			close1_ = 4;
			open1_ = 4;
			initialRangeValue_ = 90;
			rangeWidth_ = 32;
			open2_ = 8;
			close2_ = 4;
			break;
		case YELLOW:
			thresh0_ = 50;
			erosion0_ = 1;
			thresh1_ = 60;
			close1_ = 4;
			open1_ = 4;
			initialRangeValue_ = 90;
			rangeWidth_ = 32;
			open2_ = 8;
			close2_ = 4;
	}
	color_ = color;
}
/*

void RobotinoVision::processImageLampPost()
{
	Mat imgBGR;
	cvtColor(imgRGB_, imgBGR, CV_RGB2BGR);
	imshow("BRG Model", imgBGR);
	Mat imgHSL;
	cvtColor(imgRGB_, imgHSL, CV_RGB2HLS);
	imshow("HSL Model", imgHSL);
	Mat splittedImgHSL[3];
	split(imgHSL, splittedImgHSL);
	Mat imgH = splittedImgHSL[0];
	imshow("Hue", imgH);
	Mat imgS = splittedImgHSL[1];
	imshow("Saturation", imgS);
	Mat imgL = splittedImgHSL[2];
	imshow("Lightness", imgL);
	
	Mat threshImgL;
	namedWindow("Lightness Window", 1);
	createTrackbar("Lightness threshold value:", "Lightness Window", &threshVal_, 255);
	int maxVal = 255;
	threshold(imgL, threshImgL, threshVal_, maxVal, THRESH_BINARY);
//	std::vector<std::vector<Point>> contours;
//	std::vector<Vec4i> hierarchy;
//	findContours(threshImgL, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
//	Mat imgBorders = Mat::zeros(threshImgL.size(), CV_8U);
//	for (int i = 0; i < contours.size(); i++)
//		drawContours(imgBorders, contours, i, Scalar(255), 2, 8, hierarchy, 0, Point());
//	imshow("Contours", imgBorders);
	//imshow("Lightness Window", threshImgL);
	
	/*const int maskSize = 7;
	int mask[maskSize][maskSize] = {{0, 0, 0, 0, 0, 0, 0},
					{0, 0, 0, 0, 0, 0, 0},
					{0, 0, 0, 0, 0, 0, 0}, 
					{1, 1, 1, 0, 1, 1, 1},
					{0, 0, 0, 0, 0, 0, 0},
					{0, 0, 0, 0, 0, 0, 0}, 
					{0, 0, 0, 0, 0, 0, 0}};  
	Mat kernel = Mat(maskSize, maskSize, CV_8U, (void*) mask, maskSize);*/
/*	namedWindow("Dilated Lightness", 1);
	createTrackbar("Dilation Size", "Dilated Lightness", &dilationSize_, 10);
	Mat element = getStructuringElement(MORPH_RECT, Size(2 * dilationSize_ + 1, 2 * dilationSize_ + 1), Point(dilationSize_, dilationSize_)); */
	/*std::sstream == "";
	for (t_size i = 0; i < element.rows; i++)
	{
		for (t_size j = 0; j < element.columns; j++)
			s << 
		s << std::endl;
	}	*/
/*
	Mat dilatedImgL;
	dilate(threshImgL, dilatedImgL, element);
	imshow("Dilated Lightness", dilatedImgL);
	
	Mat mask = dilatedImgL - threshImgL;
	imshow("Mask", mask);
	mask /= 255;*/
/*
	Mat imgH1, imgH2; 
	imgH.copyTo(imgH1);//, CV_8U);
	Scalar s1 = Scalar(25);
	imgH1 += s1;
	//std::cout << imgH1;
	imgH.copyTo(imgH2);//, CV_8U;
	Scalar s2 = Scalar(230);
	imgH2 -= s2;
	Mat newImgH = imgH1 + imgH2;*/
/*	vector<Mat> masks;
	masks.push_back(mask);
	masks.push_back(mask);
	masks.push_back(mask);
	Mat imgMask;
	merge(masks, imgMask);
	Mat maskedImgBGR;
	maskedImgBGR = imgBGR.mul(imgMask);
	Mat finalImgH = imgH.mul(mask); 
	imshow("Final Filter", maskedImgBGR);

	Mat imgRed, imgRed1, imgRed2, imgRed3, imgRed4;
	//int redThreshMinVal1 = 0;
	//int redThreshMaxVal1 = 10;
	namedWindow("Red Range", 1);
	createTrackbar("Range1MinVal", "Red Range", &redThreshMinVal1_, 255);
	createTrackbar("Range1MaxVal", "Red Range", &redThreshMaxVal1_, 255);
	int redMaxVal = 1;
	threshold(finalImgH, imgRed1, redThreshMinVal1_, redMaxVal, THRESH_BINARY);
	threshold(finalImgH, imgRed2, redThreshMaxVal1_, redMaxVal, THRESH_BINARY_INV);
	imgRed3 = imgRed1.mul(imgRed2); 
	//int redThreshMinVal2 = 228;
	//int redThreshMaxVal2 = 255;
	createTrackbar("Range2MinVal", "Red Range", &redThreshMinVal2_, 255);
	createTrackbar("Range2MaxVal", "Red Range", &redThreshMaxVal2_, 255);
	threshold(finalImgH, imgRed1, redThreshMinVal2_, redMaxVal, THRESH_BINARY);
	threshold(finalImgH, imgRed2, redThreshMaxVal2_, redMaxVal, THRESH_BINARY_INV);
	imgRed4 = imgRed1.mul(imgRed2);
	imgRed = (imgRed3 + imgRed4) * 255;
	imshow("Red Range", imgRed);

	Mat imgYellow, imgYellow1, imgYellow2;
	//int yellowThreshMinVal = 15;
	//int yellowThreshMaxVal = 55;
	namedWindow("Yellow Range", 1);
	createTrackbar("RangeMinVal", "Yellow Range", &yellowThreshMinVal_, 255);
	createTrackbar("RangeMaxVal", "Yellow Range", &yellowThreshMaxVal_, 255);
	int yellowMaxVal = 1;
	threshold(finalImgH, imgYellow1, yellowThreshMinVal_, yellowMaxVal, THRESH_BINARY);
	threshold(finalImgH, imgYellow2, yellowThreshMaxVal_, yellowMaxVal, THRESH_BINARY_INV);
	imgYellow = imgYellow1.mul(imgYellow2) * 255;
	imshow("Yellow Range", imgYellow);
	
	Mat imgGreen, imgGreen1, imgGreen2;
	//int greenThreshMinVal = 58;
	//int greenThreshMaxVal = 96;
	namedWindow("Green Range", 1);
	createTrackbar("RangeMinVal", "Green Range", &greenThreshMinVal_, 255);
	createTrackbar("RangeMaxVal", "Green Range", &greenThreshMaxVal_, 255);
	int greenMaxVal = 1;
	threshold(finalImgH, imgGreen1, greenThreshMinVal_, greenMaxVal, THRESH_BINARY);
	threshold(finalImgH, imgGreen2, greenThreshMaxVal_, greenMaxVal, THRESH_BINARY_INV);
	imgGreen = imgGreen1.mul(imgGreen2) * 255;
	imshow("Green Range", imgGreen);
	
	waitKey(80);
}*/

