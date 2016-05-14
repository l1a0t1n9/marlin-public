#include "buoys/buoys.hpp"

//Creates an equalized distribution of certain colors
//Read more at http://docs.opencv.org/2.4/doc/tutorials/imgproc/histograms/histogram_equalization/histogram_equalization.html
cv::Mat equalColorHist(cv::Mat& img, bool red, bool green, bool blue)
{
	std::vector<cv::Mat> channels;
	cv::split(img, channels);

	if (blue)
	{
		cv::equalizeHist(channels[0], channels[0]);
	}
	if (green)
	{
		cv::equalizeHist(channels[1], channels[1]);
	}
	if (red)
	{
		cv::equalizeHist(channels[2], channels[2]);
	}

	cv::Mat result;
	cv::merge(channels, result);
	return result;
}

//Turns the mat into a diffmap (make each pixel the difference between it and its neighbors)
//Useful because the water near the top usually looks like the green buoys near the bottom
cv::Mat generateDiffMap(cv::Mat& img, int diff)
{
	cv::Mat diffMap = cv::Mat(img.size(), CV_32F, cv::Scalar(0));
	float* ip = img.ptr<float>();
	float* op = diffMap.ptr<float>();
	auto getDiffs = [=](int xBeg, int xEnd, int yBeg, int yEnd)
	{
		for (int c = xBeg; c < xEnd; c++)
		{
			for (int r = yBeg; r < yEnd; r++)
			{
				// make the value equal to how much it stands out from its horizontal or vertical neighbors, whichever is less
				float vert = (2*ip[(r*img.cols+c)]-ip[((r-diff)*img.cols+c)]-ip[((r+diff)*img.cols+c)]);
				float hori = (2*ip[(r*img.cols+c)]-ip[(r*img.cols+c+diff)]-ip[(r*img.cols+c-diff)]);
				float weak = (std::abs(vert) < std::abs(hori)) ? vert : hori;
				op[r*img.cols+c] = weak;
			}
		}
	};
	getDiffs(diff,img.cols-diff,diff,img.rows-diff);
 
	return diffMap;
}
	
//These values were originally in config for l_gate but i don't see a need for them to be configurable. 
//If you would like them to be configurable, talk to Varun. If Varun has graduated/quit for the Blue Devils by this point, I wish you the best of luck
//This is Evan. I think the point is to make basically all the constants configurable if need be. I will make them configurable. Later.
float cropx = 1.0;
float cropy = 1.0;
float offset = .9 * (1-cropy);
float scalex = 0.2;
float scaley = 0.2;
int diffDist = 8;
//End supposedly config values that I suspect were never acually modified

float rx, ry, gx, gy, yx, yy;
//this was global in the original buoys so stop blaming me luk
cv::Mat processedImage; 

int main(int argc, char** argv) {
	//offset = 0;
	// obtain images from the front camera.
 
	//TODO: replace with interface thing.
	cv::Mat image = cv::imread(argv[1], CV_LOAD_IMAGE_COLOR);

	if(! image.data ){
	std::cout <<  "Could not open or find the image" << std::endl ;
	return -1;
	}
	// reset processedImage and define the image size
 
	processedImage.create(360, 640, CV_8UC3);

	memset(processedImage.ptr(), 100, processedImage.rows*processedImage.cols*3);
	int image_size = image.rows*image.cols;

	// crop and resize image
	cv::resize(image(cv::Rect(image.cols*(1-cropx)/2, image.rows*(1-cropy-offset)/2, 
				image.cols*cropx, image.rows*cropy)), image, 
			cv::Size(image.cols*cropx*scalex, image.rows*cropy*scaley));

	// create temporary image to store lower smaller processed image
	cv::Mat output(image.rows, image.cols, CV_8UC3, cv::Scalar(0, 0, 0));
	unsigned char* op = output.ptr();

	cv::Mat imgR, imgG, imgY;
	imgR.create(image.rows, image.cols, CV_8UC1);
	imgG.create(image.rows, image.cols, CV_8UC1);
	imgY.create(image.rows, image.cols, CV_8UC1);

	unsigned char *rPtr = imgR.ptr(), *gPtr = imgG.ptr(), *yPtr = imgY.ptr();

	const unsigned char* ip = image.ptr();

	// filter for green
	cv::Mat grMat(image.size(), CV_32F, cv::Scalar(0));
	cv::Mat reMat(image.size(), CV_32F, cv::Scalar(0));
	float* gp = grMat.ptr<float>();
	float* rp = reMat.ptr<float>();
	cv::Mat histo = equalColorHist(image, false, false, false);
	unsigned char* hp = histo.ptr();
	auto filterColors = [=](int beg, int end)
	{
		for (int i = 0; i < image.rows*image.cols; i++)
		{
			int b = hp[3*i], g = hp[3*i+1], r = (int)hp[3*i+2];
			//gp[i] = 40.0*(g-0.4*r)/(b+3.0*r+10.1);
			gp[i] = (g+0.4*r)/(b+0.7*r+3.01)-0.004*r;
			rp[i] = 70.0*r/(g+1.0) - 120.0*g/(b+1.0)-(g+b)*0.1;
		}
	};
 
	filterColors(0 , image.rows * image.cols);
	cv::Mat diMap = generateDiffMap(grMat, diffDist);

	cv::Mat drMap = generateDiffMap(reMat, diffDist);
	float* drffp = drMap.ptr<float>();

	cv::blur(diMap, diMap, cv::Size(3,3));
	float* diffp = diMap.ptr<float>();
	float maxR[4][4] = {{-1000, 0, 0, 1000}};
	int maxY[4][3] = {{-1}};
	float maxG[4][4] = {{-1000, 0, 0, 1000}};
	auto filterRY = [=](int start, int end, float rVals[4], int yVals[3], float gVals[4])
	{
		for (int i = 0; i < image.cols*image.rows; i++)
		{
			int b = ip[3*i], g = ip[3*i+1], r = (int)ip[3*i+2];
			
			op[3*i+2] = drffp[i]/10+128;
			// filter for red
			if (drffp[i] > rVals[0])
			{
				rVals[0] = drffp[i];
				rVals[1] = i % image.cols;
				rVals[2] = i / image.cols;
			}
			if (drffp[i] < rVals[3])
			{
				rVals[3] = drffp[i];
			}

			// filter for yellow
			yPtr[i] = std::max(0, std::min(255, 128+(r+g-b)/4));
			op[3*i] = yPtr[i];

			// find yellow buoy
			if (yPtr[i] > yVals[0])
			{
				yVals[0] = yPtr[i];
				yVals[1] = i % image.cols;
				yVals[2] = i / image.cols;
			}

			op[3*i+1] = diffp[i]+128;
			//find green buoy and max/min values
			if (diffp[i] > gVals[0])
			{
				gVals[0] = diffp[i];
				gVals[1] = i % image.cols;
				gVals[2] = i / image.cols;
			}
			if (diffp[i] < gVals[3])
			{
				gVals[3] = diffp[i];
			}
		}
	};
	filterRY( 0, image.rows*image.cols/4, maxR[0], maxY[0], maxG[0]);
	filterRY( image.rows*image.cols/4, 2*image.rows*image.cols/4, maxR[1], maxY[1], maxG[1]);
	filterRY( 2*image.rows*image.cols/4, 3*image.rows*image.cols/4, maxR[2], maxY[2], maxG[2]);
	filterRY( 3*image.rows*image.cols/4, image.rows*image.cols, maxR[3], maxY[3], maxG[3]);
	float rMax = -2;
	float rMin = -2;
	int yMax = -2;
	float gMax = -2;
	float gMin = -2;
	for (int i = 0; i < 4; i++)
	{
		if (maxR[i][0] > rMax)
		{
			rMax = maxR[i][0];
			rx = (int)maxR[i][1];
			ry = (int)maxR[i][2];
		}
		if (maxR[i][3] < rMin)
		{
			rMin = maxR[i][3];
		}
		if (maxY[i][0] > yMax)
		{
			yMax = maxY[i][0];
			yx = maxY[i][1];
			yy = maxY[i][2];
		}
		if (maxG[i][0] > gMax)
		{
			gMax = maxG[i][0];
			gx = (int)maxG[i][1];
			gy = (int)maxG[i][2];
		}
		if (maxG[i][3] < gMin)
		{
			gMin = maxG[i][3];
		}
	}

	// highlight buoys on processed image
	op[(int)(3*(ry*image.cols+rx))] = 0;
	op[(int)(3*(ry*image.cols+rx))+1] = 0;
	op[(int)(3*(gy*image.cols+gx))] = 0;
	op[(int)(3*(gy*image.cols+gx))+2] = 0;
	op[(int)(3*(yy*image.cols+yx))+1] = 0;
	op[(int)(3*(yy*image.cols+yx))+2] = 0;
	op[(int)(3*(gy*image.cols+gx))+1] = 255;
	op[(int)(3*(ry*image.cols+rx))+2] = 255;
	op[(int)(3*(yy*image.cols+yx))] = 255;

	// resize filtered image for processed image
	cv::resize(output, processedImage(cv::Rect(processedImage.cols*(1-cropx)/2, processedImage.rows*(1-cropy-offset)/2, 
				processedImage.cols*cropx, processedImage.rows*cropy)), 
			cv::Size(processedImage.cols*cropx, processedImage.rows*cropy), 0, 0, cv::INTER_NEAREST);

	rx = (rx - image.cols/2) / image.cols;
	ry = (image.rows/2 - ry) / image.rows;
	gx = (gx - image.cols/2) / image.cols;
	gy = (image.rows/2 - gy) / image.rows+offset;
	yx = (yx - image.cols/2) / image.cols;
	yy = (image.rows/2 - yy) / image.rows;

	std::cout << "buoys: " << rx << " " << ry << " " << gx << " " << gy << " " << yx << " " << yy << "\n "; 
 
	return 0;
}