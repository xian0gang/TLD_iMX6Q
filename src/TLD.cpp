/*
 * TLD.cpp
 *
 *  Created on: Jun 9, 2011
 *      Author: alantrrs
 */

#include <TLD.h>
#include <stdio.h>
using namespace cv;
using namespace std;

int dddt;
extern unsigned short CCD_IR_Detect_x, CCD_IR_Detect_y;
extern int relDistanceX;
extern int relDistanceY;
cv::Mat iisum;
cv::Mat iisqsum;
///Parameters
//下面这些参数通过程序开始运行时读入parameters.yml文件进行初始化
int bbox_step;
int min_win;
int patch_size;

//initial parameters for positive examples
int num_closest_init;
int num_warps_init;
int noise_init;
float angle_init;
float shift_init;
float scale_init;
//update parameters for positive examples
int num_closest_update;
int num_warps_update;
int noise_update;
float angle_update;
float shift_update;
float scale_update;
//parameters for negative examples
float bad_overlap;
float bad_patches;
///Variables
//Integral Images


float maxx;
float sec_max;
float th_max;
int maxx_num;
int sec_max_num;
int th_max_num;
int grid_count;
int track_count;

FerNNClassifier classifier;
LKTracker tracker;
cv::PatchGenerator generator;
  //Bounding Boxes
std::vector<BoundingBox> grid;
std::vector<cv::Size> scales;
std::vector<int> good_boxes; //indexes of bboxes with overlap > 0.6
std::vector<int> bad_boxes; //indexes of bboxes with overlap < 0.2
BoundingBox bbhull; // hull of good_boxes
BoundingBox best_box; // maximum overlapping bbox

float var;
//Training data
std::vector<std::pair<std::vector<int>,int> > pX; //positive ferns <features,labels=1>
std::vector<std::pair<std::vector<int>,int> > nX; // negative ferns <features,labels=0>
cv::Mat pEx;  //positive NN example
std::vector<cv::Mat> nEx; //negative NN examples
//Test data
std::vector<std::pair<std::vector<int>,int> > nXT; //negative data to Test
std::vector<cv::Mat> nExT; //negative NN examples to Test
//Last frame data
BoundingBox lastbox;
bool lastvalid;
float lastconf;
//Current frame data
//Tracker data
bool tracked;
BoundingBox tbb;
bool tvalid;
float tconf;
//Detector data
TempStruct tmp;
DetStruct dt;
std::vector<BoundingBox> dbb;
std::vector<bool> dvalid;
std::vector<float> dconf;
bool detected;

void clearr()
{
    DEBLOG("clear start\n");
	pX.clear();
	nX.clear();
	nXT.clear();
	nEx.clear();
	nExT.clear();
	dbb.clear();
	dconf.clear();
	grid.clear();
	scales.clear();
	good_boxes.clear();
	bad_boxes.clear();
    classifier.clear();
    DEBLOG("clear over\n");
}




//多线程变量
int summ;
bool isdetect;
static void *pth_test(void *);
int ppp();
std::mutex mut;
std::condition_variable data_cond;
Mat dec_mat;
Mat img_det;
bool isdetect_det;
std::mutex mut_det;
std::condition_variable data_cond_det;


/* detect相似度第二个线程控制  */
int g_idx2;

bool isdetect2;
std::mutex mut_det2;
std::condition_variable data_cond_det2;
bool isdetect2_2;
std::mutex mut_det2_2;
std::condition_variable data_cond_det2_2;
static void *pth_det2(void *);
int detect_d2();

/* detect相似度第三个线程控制  */
int g_idx3;

bool isdetect3;
std::mutex mut_det3;
std::condition_variable data_cond_det3;
bool isdetect3_3;
std::mutex mut_det3_3;
std::condition_variable data_cond_det3_3;
static void *pth_det3(void *);
int detect_d3();





/* detect 方差计算线程  */
int g_idx4;
Mat img_g;

bool isdetect4;
std::mutex mut_det4;
std::condition_variable data_cond_det4;
bool isdetect4_4;
std::mutex mut_det4_4;
std::condition_variable data_cond_det4_4;
static void *pth_det4(void *);
int detect_d4();



//******************************************************
pthread_t pthppp;
pthread_t pth2;
pthread_t pth3;
pthread_t pth4;

//线程退出条件 1 不退出 0 退出线程
unsigned char Pth_destory_Flag = 1;

//detect检测模块线程函数
void  *pth_test(void *tt)
{
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    int s,core_no = 2;
    CPU_ZERO(&cpuset);
    CPU_SET(core_no,&cpuset);
    s = pthread_setaffinity_np(thread,sizeof(cpuset),&cpuset);
    if(s != 0)
        DEBLOG("Set pthread_setaffinity_npfailed\n");

    int count = 0;
    while(Pth_destory_Flag)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, []{return isdetect;});

        // sleep(2);
        count++;
        double t = (double)getTickCount();
        detect(dec_mat);
        t=(double)getTickCount()-t;
        DEBLOG("xiangang----------------------------------->detect Run-time: %gms\n", t*1000/getTickFrequency());

        data_cond.notify_one();
        isdetect = false;
        lk.unlock();
    }
    DEBLOG("**************************************************destory pth_test\n");
}

//detect检测模块显存封装函数
int ppp()
{
    // pthread_t pth;
    pthread_create(&pthppp, NULL, pth_test,NULL);
	//pthread_join(pth,NULL);
}


void  *pth_det2(void *tt)
{
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    int s,core_no = 1;
    CPU_ZERO(&cpuset);
    CPU_SET(core_no,&cpuset);
    s = pthread_setaffinity_np(thread,sizeof(cpuset),&cpuset);
    if(s != 0)
        DEBLOG("Set pthread_setaffinity_npfailed\n");


    // DetStruct dtt;    
    // dtt.patt = vector<vector<int> >(1,vector<int>(10,0));        //  Corresponding codes of the Ensemble Classifier
    // dtt.conf1 = vector<float>(1);                                //  Relative Similarity (for final nearest neighbour classifier)
    // dtt.conf2 =vector<float>(1);                                 //  Conservative Similarity (for integration with tracker)
    // dtt.isin = vector<vector<int> >(1,vector<int>(3,-1));        //  Detected (isin=1) or rejected (isin=0) by nearest neighbour classifier
    // dtt.patch = vector<Mat>(1,Mat(patch_size,patch_size,/*CV_32F*/CV_8U));//  Corresponding patches

        while(Pth_destory_Flag)
        {
//lock
            std::unique_lock<std::mutex> lk2(mut_det2);
            data_cond_det2.wait(lk2, []{return isdetect2;});
// main
            // int idx;
            // Scalar mean, stdev;
            // float nn_th = classifier.getNNTh();
           // nn_th = 0.5;
            // Mat patch;

            // idx=g_idx2;                                                       //  Get the detected bounding box index

            // patch = dec_mat(grid[idx]);

            // getPattern(patch,dtt.patch[0],mean,stdev);                //  Get pattern within bounding box

            // classifier.NNConf(dtt.patch[0],dtt.isin[0],dtt.conf1[0],dtt.conf2[0]);  //  Evaluate nearest neighbour classifier
            // dtt.patt[0]=tmp.patt[idx];
     
            // if (dtt.conf1[0]>nn_th)
            // {                                               //  idx = dt.conf1 > tld.model.thr_nn; % get all indexes that made it through the nearest neighbour
                // BoundingBox temp;
                // temp = grid[idx];
                // dbb.push_back(temp);                                            //  BB    = dt.bb(:,idx); % bounding boxes
                // dconf.push_back(dtt.conf2[0]);                                     //  Conf  = dt.conf2(:,idx); % conservative confidences
            // }
              
            // dtt.bb.clear();  

            // std::lock_guard<std::mutex> lk2_2(mut_det2_2);
            // isdetect2_2 = true;
            // data_cond_det2_2.notify_one();
            
            learn(/*img2*/dec_mat);
            
            isdetect2 = false;
            lk2.unlock();
        }
        DEBLOG("**************************************************destory pth_test2\n");
		
}

int detect_d2()
{
   pthread_t pth;
    pthread_create(&pth, NULL, pth_det2,NULL);
	//pthread_join(pth,NULL);
}

void  *pth_det3(void *tt)
{

    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    int s,core_no = 1;
    CPU_ZERO(&cpuset);
    CPU_SET(core_no,&cpuset);
    s = pthread_setaffinity_np(thread,sizeof(cpuset),&cpuset);
    if(s != 0)
        DEBLOG("Set pthread_setaffinity_npfailed\n");

    DetStruct dtt4;    
    dtt4.patt = vector<vector<int> >(1,vector<int>(10,0));        //  Corresponding codes of the Ensemble Classifier
    dtt4.conf1 = vector<float>(1);                                //  Relative Similarity (for final nearest neighbour classifier)
    dtt4.conf2 =vector<float>(1);                                 //  Conservative Similarity (for integration with tracker)
    dtt4.isin = vector<vector<int> >(1,vector<int>(3,-1));        //  Detected (isin=1) or rejected (isin=0) by nearest neighbour classifier
    dtt4.patch = vector<Mat>(1,Mat(patch_size,patch_size,CV_32F));//  Corresponding patches

        while(Pth_destory_Flag)
        {
        //lock
            std::unique_lock<std::mutex> lk3(mut_det3);
            data_cond_det3.wait(lk3, []{return isdetect3;});
// main
            int idx;
            Scalar mean, stdev;
            float nn_th = classifier.getNNTh();
//            nn_th = 0.5;
            Mat patch;

            idx=g_idx3;                                                       //  Get the detected bounding box index

            patch = dec_mat(grid[idx]);

            getPattern(patch,dtt4.patch[0],mean,stdev);                //  Get pattern within bounding box

            classifier.NNConf(dtt4.patch[0],dtt4.isin[0],dtt4.conf1[0],dtt4.conf2[0]);  //  Evaluate nearest neighbour classifier
            dtt4.patt[0]=tmp.patt[idx];
     
            if (dtt4.conf1[0]>nn_th)
            {                                               //  idx = dt.conf1 > tld.model.thr_nn; % get all indexes that made it through the nearest neighbour
                BoundingBox temp;
                temp = grid[idx];
                dbb.push_back(temp);        
                dconf.push_back(dtt4.conf2[0]);                                     //  Conf  = dt.conf2(:,idx); % conservative confidences
            }
              
            dtt4.bb.clear();  

            std::lock_guard<std::mutex> lk3_3(mut_det3_3);
            isdetect3_3 = true;
            data_cond_det3_3.notify_one();

            isdetect3 = false;
            lk3.unlock();
        }
        DEBLOG("**************************************************destory pth_test3\n");
}

int detect_d3()
{
    pthread_t pth;
    pthread_create(&pth, NULL, pth_det3,NULL);
	//pthread_join(pth,NULL);
}


void  *pth_det4(void *tt)
{

    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    int s,core_no = 1;
    CPU_ZERO(&cpuset);
    CPU_SET(core_no,&cpuset);
    s = pthread_setaffinity_np(thread,sizeof(cpuset),&cpuset);
    if(s != 0)
        DEBLOG("Set pthread_setaffinity_npfailed\n");

    DEBLOG("pth_det4......\n");
        while(Pth_destory_Flag)
        {
//lock
            std::unique_lock<std::mutex> lk4(mut_det4);
//            DEBLOG("pth_det4 wait......\n");
            data_cond_det4.wait(lk4, []{return isdetect4;});
            isdetect4 = false;
            lk4.unlock();
// main
            //        DEBLOG("pth_grid1 start while......\n");
            float conf;
            Mat patch;
            int numtrees = classifier.getNumStructs();
            float fern_th = classifier.getFernTh();
            vector <int> ferns(10);
            float conf_pro = numtrees*fern_th;
           double t = (double)getTickCount();

    //      main

            int grid_len = grid.size();
            int half = grid_len / 2;
            for (int i = half;i<grid_len;i++)
            {
                grid_count++;
                if (getVar(grid[i],iisum,iisqsum)>=var)
                {
                    patch = img_g(grid[i]);
                    classifier.getFeatures(patch,grid[i].sidx,ferns);
                    conf = classifier.measure_forest(ferns);

                    if (conf>conf_pro)
                    {
                        //只取前三位conf
                            dt.bb.push_back(i);
//                        }
                    }
                }
            }

           t=(double)getTickCount() - t;
           DEBLOG("xiangang----------------------------------->detect2 grid-time: %gms\n", t*1000/getTickFrequency());


            std::unique_lock<std::mutex> lk4_4(mut_det4_4);
            isdetect4_4 = true;
            data_cond_det4_4.notify_one();



        }

        DEBLOG("**************************************************destory pth_test4\n");
}

int detect_d4()
{
    pthread_t pth;
    pthread_create(&pth, NULL, pth_det4,NULL);
	//pthread_join(pth,NULL);
}
extern FILE *bb_file;

int TLD_Pthread_destory(void)
{
    //线程退出条件 1 不退出 0 退出线程
	Pth_destory_Flag = 0;
	// 线程可能阻塞，发信号通过
//	isdetect = true;
//	data_cond.notify_one();
//	// 等待线程退出
//	pthread_join(pthppp,NULL);
    // 清除变量容器等
//     clearr();
	
	fclose(bb_file);
}


int TLD_Pthread_create(void)
{
    //线程退出条件 1 不退出 0 退出线程
	Pth_destory_Flag = 1;

}



void read(const FileNode& file)
{
    //控制量初始化为false，线程等待
    isdetect = false;
    isdetect_det = false;
    isdetect2 = false;
    isdetect3 = false;
    isdetect4 = false;
    isdetect2_2 = false;
    isdetect3_3 = false;
    isdetect4_4 = false;
//    half_grid1 = false;
//    half_grid1_1 = false;
    track_count = 0;
    dddt = 0;

    maxx = 0;
    sec_max = 0;
    th_max = 0;
    maxx_num = 0;
    sec_max_num = 0;
    th_max_num = 0;
    grid_count = 0;

    ///Bounding Box Parameters
    min_win = (int)file["min_win"];
    ///Genarator Parameters
    //initial parameters for positive examples
    patch_size = (int)file["patch_size"];
    num_closest_init = (int)file["num_closest_init"];
    num_warps_init = (int)file["num_warps_init"];
    noise_init = (int)file["noise_init"];
    angle_init = (float)file["angle_init"];
    shift_init = (float)file["shift_init"];
    scale_init = (float)file["scale_init"];
    //update parameters for positive examples
    num_closest_update = (int)file["num_closest_update"];
    num_warps_update = (int)file["num_warps_update"];
    noise_update = (int)file["noise_update"];
    angle_update = (float)file["angle_update"];
    shift_update = (float)file["shift_update"];
    scale_update = (float)file["scale_update"];
    //parameters for negative examples
    bad_overlap = (float)file["overlap"];
    bad_patches = (int)file["num_patches"];
    classifier.read(file);
}

//此函数完成准备工作
void init(const Mat& frame1,const Rect& box,FILE* bb_file)
{

  double t = (double)getTickCount();

  buildGrid(frame1,box);

  iisum.create(frame1.rows+1,frame1.cols+1,CV_32F);
  iisqsum.create(frame1.rows+1,frame1.cols+1,CV_64F);
  

  dconf.reserve(100);
  dbb.reserve(100);
  bbox_step = 7;
  //tmp.conf.reserve(grid.size());
  tmp.conf = vector<float>(grid.size());
  tmp.patt = vector<vector<int>>(grid.size(),vector<int>(10,0));
  //tmp.patt.reserve(grid.size());
  dt.bb.reserve(grid.size());
  good_boxes.reserve(grid.size());
  bad_boxes.reserve(grid.size());
  
  pEx.create(patch_size,patch_size,CV_64F);
  generator = PatchGenerator(0,0,noise_init,true,1-scale_init,1+scale_init,-angle_init*CV_PI/180,angle_init*CV_PI/180,-angle_init*CV_PI/180,angle_init*CV_PI/180);
  

  getOverlappingBoxes(box,num_closest_init);

  lastbox=best_box;
  lastconf=1;
  lastvalid=true;

  classifier.prepare(scales);
  

  generatePositiveData(frame1,num_warps_init);
  

  cv::integral(frame1,iisum,iisqsum);
  
  //check variance
  double vr =  getVar(best_box,iisum,iisqsum)*0.5;
  cout << "check variance: " << vr << endl;
  var = vr;
  // Generate negative data
  generateNegativeData(frame1);
  
  int half = (int)nX.size()*0.5f;

  nXT.assign(nX.begin()+half, nX.end());

  nX.resize(half);
  
  ///Split Negative NN Examples into Training and Testing sets
  half = (int)nEx.size()*0.5f;
  nExT.assign(nEx.begin()+half, nEx.end());
  nEx.resize(half);
  
  vector<pair<vector<int>,int>> ferns_data(nX.size()+pX.size());
  vector<int> idx = index_shuffle(0,ferns_data.size());
  int a=0;
  for (int i=0;i<pX.size();i++){
      ferns_data[idx[a]] = pX[i];
      a++;
  }
  
  for (int i=0;i<nX.size();i++){
      ferns_data[idx[a]] = nX[i];
      a++;
  }

  vector<cv::Mat> nn_data(nEx.size()+1);
  nn_data[0] = pEx;
  for (int i=0;i<nEx.size();i++){
      nn_data[i+1]= nEx[i];
  }
  

  classifier.trainF(ferns_data,2); //bootstrap = 2
  classifier.trainNN(nn_data);
  classifier.evaluateTh(nXT,nExT);

  t=(double)getTickCount()-t;
  DEBLOG("xiangang----------------------------------->init Run-time: %gms\n", t*1000/getTickFrequency());
}

/* Generate Positive data
 * Inputs:
 * - good_boxes (bbP)
 * - best_box  (bbP0)
 * - frame (im0)
 * Outputs:
 * - Positive fern features (pX)
 * - Positive NN examples  (pEx)
 */
void generatePositiveData(const Mat& frame, int num_warps)
{
    Scalar mean;
    Scalar stdev;
    getPattern(frame(best_box),pEx,mean,stdev);
    //Get Fern features on warped patches
    Mat img;
    Mat warped;
    GaussianBlur(frame,img,Size(9,9),1.5);
    warped = img(bbhull);
    RNG& rng = theRNG();
    Point2f pt(bbhull.x+(bbhull.width-1)*0.5f,bbhull.y+(bbhull.height-1)*0.5f);
    vector<int> fern(classifier.getNumStructs());
    pX.clear();
    Mat patch;
    if (pX.capacity()<num_warps*good_boxes.size())
    pX.reserve(num_warps*good_boxes.size());
    int idx;
    for (int i=0;i<num_warps;i++)
    {
        if (i>0)
            generator(frame,pt,warped,bbhull.size(),rng);
        for (int b=0;b<good_boxes.size();b++)
        {
            idx=good_boxes[b];
            patch = img(grid[idx]);
            classifier.getFeatures(patch,grid[idx].sidx,fern);
            pX.push_back(make_pair(fern,1));
        }
    }
    DEBLOG("Positive examples generated: ferns:%d NN:1\n",(int)pX.size());
}

inline void getPattern(const Mat& img, Mat& pattern,Scalar& mean,Scalar& stdev)
{
    resize(img,pattern,Size(patch_size,patch_size));
    meanStdDev(pattern,mean,stdev);
    pattern.convertTo(pattern,CV_32F);
    pattern = pattern-mean.val[0];
}

void generateNegativeData(const Mat& frame)
{
    /* Inputs:
    * - Image
    * - bad_boxes (Boxes far from the bounding box)
    * - variance (pEx variance)
    * Outputs
    * - Negative fern features (nX)
    * - Negative NN examples (nEx)
    */
    random_shuffle(bad_boxes.begin(),bad_boxes.end());//Random shuffle bad_boxes indexes
    int idx;
    //Get Fern Features of the boxes with big variance (calculated using integral images)
    int a=0;
    //int num = std::min((int)bad_boxes.size(),(int)bad_patches*100); //limits the size of bad_boxes to try
    DEBLOG("negative data generation started.\n");
    vector<int> fern(classifier.getNumStructs());
    nX.reserve(bad_boxes.size());
    Mat patch;
    for (int j=0;j<bad_boxes.size();j++)
    {
        idx = bad_boxes[j];
        if (getVar(grid[idx],iisum,iisqsum)<var*0.5f)
            continue;
        patch =  frame(grid[idx]);
        classifier.getFeatures(patch,grid[idx].sidx,fern);
        nX.push_back(make_pair(fern,0));
        a++;
    }
    DEBLOG("Negative examples generated: ferns: %d ",a);
    //random_shuffle(bad_boxes.begin(),bad_boxes.begin()+bad_patches);//Randomly selects 'bad_patches' and get the patterns for NN;
    Scalar dum1, dum2;
    nEx=vector<Mat>(bad_patches);
    for (int i=0;i<bad_patches;i++)
    {
        idx=bad_boxes[i];
        patch = frame(grid[idx]);
        getPattern(patch,nEx[i],dum1,dum2);
    }
    DEBLOG("NN: %d\n",(int)nEx.size());
}

int getVar(const BoundingBox& box,const Mat& sum,const Mat& sqsum)
{
    int brs1 = sum.at<int>(box.y+box.height,box.x+box.width);
    int bls1 = sum.at<int>(box.y+box.height,box.x);
    int trs1 = sum.at<int>(box.y,box.x+box.width);
    int tls1 = sum.at<int>(box.y,box.x);
    int brsq1 = sqsum.at<double>(box.y+box.height,box.x+box.width);
    int blsq1 = sqsum.at<double>(box.y+box.height,box.x);
    int trsq1 = sqsum.at<double>(box.y,box.x+box.width);
    int tlsq1 = sqsum.at<double>(box.y,box.x);
    int mean1 = (brs1+tls1-trs1-bls1)/(box.area());
    int sqmean1 = (brsq1+tlsq1-trsq1-blsq1)/(box.area());
    return sqmean1-mean1*mean1;
}

void processFrame(const cv::Mat& img1,const cv::Mat& img2,vector<Point2f>& points1,vector<Point2f>& points2,BoundingBox& bbnext,bool& lastboxfound, bool tl, FILE* bb_file)
{

    double ptt = (double)getTickCount();
    vector<BoundingBox> cbb;
    vector<float> cconf;
    int confident_detections=0;
    int didx; //detection index


	Mat re_img1;
//    resize(img2, dec_mat, Size(img2.cols/4, img2.rows/4));
    dec_mat = img2(Rect(CCD_IR_Detect_x - 64, CCD_IR_Detect_y - 64, 128, 128));

//    std::unique_lock<std::mutex> lk(mut);
//    isdetect = true;
//    data_cond.notify_one();

    re_img1 = img1(Rect(CCD_IR_Detect_x - 64, CCD_IR_Detect_y - 64, 128, 128));

    if(relDistanceX != 0 || relDistanceY != 0)
    {
        lastbox.x = relDistanceX;
        lastbox.y = relDistanceY;
    }
    ///Track  跟踪模块
    if(lastboxfound && tl)
    {

        //跟踪
        double t = (double)getTickCount();
        track(re_img1,dec_mat,points1,points2);
        t=(double)getTickCount()-t;
        DEBLOG("xiangang----------------------------------->track Run-time: %gms\n", t*1000/getTickFrequency());
    }
    else
    {
        tracked = false;
    }

    ///Detect   检测模块
     detect(dec_mat);


//    data_cond.wait(lk, []{return !isdetect;});

    ///Integration 综合模块
    //TLD只跟踪单目标，所以综合模块综合跟踪器跟踪到的单个目标和检测器检测到的多个目标，然后只输出保守相似度最大的一个目标
    if(tracked)
    {
        bbnext=tbb;
    	lastconf=tconf; //表示相关相似度的阈值
    	lastvalid=tvalid;
        DEBLOG("Tracked\n");
    	if(detected)
    	{                                               //   if Detected
    		clusterConf(dbb,dconf,cbb,cconf);                       //   cluster detections
            DEBLOG("Found %d clusters\n",(int)cbb.size());
    		for (int i=0;i<cbb.size();i++)
    		{
    			if (bbOverlap(tbb,cbb[i])<0.5 && cconf[i]>tconf)
    			{  //  Get index of a clusters that is far from tracker and are more confident than the tracker
    				confident_detections++;
    				didx=i; //detection index
    			}
    		}
    		if (confident_detections==1)
    		{                                //if there is ONE such a cluster, re-initialize the tracker
                DEBLOG("Found a better match..reinitializing tracking\n");
    			bbnext=cbb[didx];
    			lastconf=cconf[didx];
    			lastvalid=false;
    		}
    		else
    		{
                DEBLOG("%d confident cluster was found\n",confident_detections);
    			int cx=0,cy=0,cw=0,ch=0;
    			int close_detections=0;
    			for (int i=0;i<dbb.size();i++)
    			{
    				if(bbOverlap(tbb,dbb[i])>0.7)
    				{                     // Get mean of close detections
    				  cx += dbb[i].x;
    				  cy +=dbb[i].y;
    				  cw += dbb[i].width;
    				  ch += dbb[i].height;
    				  close_detections++;
                      DEBLOG("weighted detection: %d %d %d %d\n",dbb[i].x,dbb[i].y,dbb[i].width,dbb[i].height);
    				}
    			}
    			if (close_detections>0)
    			{
    				//对与跟踪器预测到的box距离很近的box 和 跟踪器本身预测到的box 进行坐标与大小的平均作为最终的
    			  //目标bounding box，但是跟踪器的权值较大
    				bbnext.x = cvRound((float)(10*tbb.x+cx)/(float)(10+close_detections));   // weighted average trackers trajectory with the close detections
    				bbnext.y = cvRound((float)(10*tbb.y+cy)/(float)(10+close_detections));
    				bbnext.width = cvRound((float)(10*tbb.width+cw)/(float)(10+close_detections));
    				bbnext.height =  cvRound((float)(10*tbb.height+ch)/(float)(10+close_detections));
                    DEBLOG("Tracker bb: %d %d %d %d\n",tbb.x,tbb.y,tbb.width,tbb.height);
                    DEBLOG("Average bb: %d %d %d %d\n",bbnext.x,bbnext.y,bbnext.width,bbnext.height);
                    DEBLOG("Weighting %d close detection(s) with tracker..\n",close_detections);
    			}
    			else
    			{
                    DEBLOG("%d close detections were found\n",close_detections);

    			}
    		}
    	}
    }
    else  //   If NOT tracking
    {                                       //   If NOT tracking
        DEBLOG("Not tracking..\n");
    	lastboxfound = false;
    	lastvalid = false;
    	 //如果跟踪器没有跟踪到目标，但是检测器检测到了一些可能的目标box，那么同样对其进行聚类，但只是简单的
      //将聚类的cbb[0]作为新的跟踪目标box（不比较相似度了？？还是里面已经排好序了？？），重新初始化跟踪器
    	if(detected)
    	{                           //  and detector is defined
    		clusterConf(dbb,dconf,cbb,cconf);   //  cluster detections
            DEBLOG("Found %d clusters\n",(int)cbb.size());
    		if (cconf.size()==1)
    		{
    			bbnext=cbb[0];
    			lastconf=cconf[0];
                DEBLOG("Confident detection..reinitializing tracker\n");
    			lastboxfound = true;
    		}
    	}
    }
    lastbox=bbnext;
    // if (lastboxfound)
    // 	fDEBLOG(bb_file,"%d,%d,%d,%d,%f\n",lastbox.x,lastbox.y,lastbox.br().x,lastbox.br().y,lastconf);
    // else
    // 	fDEBLOG(bb_file,"NaN,NaN,NaN,NaN,NaN\n");



    ///learn 学习模块
    if (lastvalid && tl)
    {
        learn(/*img2*/dec_mat);
//              std::lock_guard<std::mutex> lk2(mut_det2);
//       isdetect2 = true;
//       data_cond_det2.notify_one();
    }

    ptt=(double)getTickCount()-ptt;
    DEBLOG("xiangang----------------------------------->processFrame Run-time: %gms\n", ptt*1000/getTickFrequency());
}


void track(const Mat& img1, const Mat& img2,vector<Point2f>& points1,vector<Point2f>& points2)
{
	/*Inputs:
	* -current frame(img2), last frame(img1), last Bbox(bbox_f[0]).
	*Outputs:
	*- Confidence(tconf), Predicted bounding box(tbb),Validity(tvalid), points2 (for display purposes only)
	*/
	//Generate points
	//网格均匀撒点（均匀采样），在lastbox中共产生最多10*10=100个特征点，存于points
	bbPoints(points1,lastbox);
	if (points1.size()<1)
	{
        DEBLOG("BB= %d %d %d %d, Points not generated\n",lastbox.x,lastbox.y,lastbox.width,lastbox.height);
		tvalid=false;
		tracked=false;
		return;
	}
	vector<Point2f> points = points1;
	
	//Frame-to-frame tracking with forward-backward error cheking
	//trackf2f函数完成：跟踪、计算FB error和匹配相似度sim，然后筛选出 FB_error[i] <= median(FB_error) 和 
	//sim_error[i] > median(sim_error) 的特征点（跟踪结果不好的特征点），剩下的是不到50%的特征点
	//Frame-to-frame tracking with forward-backward error cheking
	tracked = tracker.trackf2f(img1,img2,points,points2);
	if (tracked)
	{
		//Bounding box prediction
		//利用剩下的这不到一半的跟踪点输入来预测bounding box在当前帧的位置和大小 tbb
		bbPredict(points,points2,lastbox,tbb);
		
		//跟踪失败检测：如果FB error的中值大于10个像素（经验值），或者预测到的当前box的位置移出图像，则
		//认为跟踪错误，此时不返回bounding box；Rect::br()返回的是右下角的坐标
		//getFB()返回的是FB error的中值
		if (tracker.getFB() > 10 || tbb.x>img2.cols ||  tbb.y>img2.rows || tbb.br().x < 1 || tbb.br().y <1)
		{
			tvalid =false; //too unstable prediction or bounding box out of image
			tracked = false;
            DEBLOG("Too unstable predictions FB error=%f\n",tracker.getFB());
			return;
		}

		//Estimate Confidence and Validity
		//评估跟踪确信度和有效性
		Mat pattern;
		Scalar mean, stdev;
		BoundingBox bb;
		bb.x = max(tbb.x,0);
		bb.y = max(tbb.y,0);
		bb.width = min(min(img2.cols-tbb.x,tbb.width),min(tbb.width,tbb.br().x));
		bb.height = min(min(img2.rows-tbb.y,tbb.height),min(tbb.height,tbb.br().y));
		
		//归一化img2(bb)对应的patch的size（放缩至patch_size = 15*15），存入pattern
		getPattern(img2(bb),pattern,mean,stdev);
		vector<int> isin;
		float dummy;
		
		//计算图像片pattern到在线模型M的保守相似度
		classifier.NNConf(pattern,isin,dummy,tconf); //Conservative Similarity
		tvalid = lastvalid;
		
		//保守相似度大于阈值，则评估跟踪有效
		if (tconf>classifier.thr_nn_valid)
		{
			tvalid =true;
		}

	}
	else
        DEBLOG("No points tracked\n");

    // DEBLOG("xiangang->track->summ:%d\n", summ);

}

//网格均匀撒点，box共10*10=100个特征点
void bbPoints(vector<cv::Point2f>& points,const BoundingBox& bb)
{
    int max_pts = 10;
    int margin_h = 0;
    int margin_v = 0;
    int stepx = ceil((bb.width-2*margin_h)/max_pts);
    int stepy = ceil((bb.height-2*margin_v)/max_pts);
    for (int y=bb.y+margin_v; y<bb.y+bb.height-margin_v; y+=stepy)
    {
        for (int x=bb.x+margin_h;x<bb.x+bb.width-margin_h;x+=stepx)
        {
            points.push_back(Point2f(x,y));
        }
    }
}

//利用剩下的这不到一半的跟踪点输入来预测bounding box在当前帧的位置和大小
void bbPredict(const vector<cv::Point2f>& points1,const vector<cv::Point2f>& points2,
                    const BoundingBox& bb1,BoundingBox& bb2)    
{
    int npoints = (int)points1.size();
    vector<float> xoff(npoints);
    vector<float> yoff(npoints);
    DEBLOG("tracked points : %d\n",npoints);
    for (int i=0;i<npoints;i++)
    {
        xoff[i]=points2[i].x-points1[i].x;
        yoff[i]=points2[i].y-points1[i].y;
    }
    float dx = median(xoff);
    float dy = median(yoff);
    float s;
    if (npoints>1)
    {
        vector<float> d;
        d.reserve(npoints*(npoints-1)/2);
        for (int i=0;i<npoints;i++)
        {
            for (int j=i+1;j<npoints;j++)
            {
                d.push_back(norm(points2[i]-points2[j])/norm(points1[i]-points1[j]));
            }
        }
        s = median(d);
    }
    else 
    {
        s = 1.0;
    }
    float s1 = 0.5*(s-1)*bb1.width;
    float s2 = 0.5*(s-1)*bb1.height;
    DEBLOG("s= %f s1= %f s2= %f \n",s,s1,s2);
    bb2.x = round( bb1.x + dx -s1);
    bb2.y = round( bb1.y + dy -s2);
    bb2.width = round(bb1.width*s);
    bb2.height = round(bb1.height*s);
    DEBLOG("predicted bb: %d %d %d %d\n",bb2.x,bb2.y,bb2.br().x,bb2.br().y);
}

void detect(const cv::Mat& frame)
{
    dbb.clear();
    dconf.clear();
    dt.bb.clear();

    img_g.create(frame.rows,frame.cols,CV_8U);
    integral(frame,iisum,iisqsum);
    GaussianBlur(frame, img_g, Size(9,9),1.5);
    int numtrees = classifier.getNumStructs();
    float fern_th = classifier.getFernTh();
    vector <int> ferns(10);
    float conf_pro = numtrees*fern_th;
    float conf;
    int a=0;
    Mat patch;
    int grid_len = grid.size();
    int half = grid_len / 2;


//    isdetect4 = true;
//    data_cond_det4.notify_one();

    for (int i=0;i<grid_len;i++)
    {
        grid_count++;
        if (getVar(grid[i],iisum,iisqsum)>=var)
        {
            a++;
            patch = img_g(grid[i]);
            classifier.getFeatures(patch,grid[i].sidx,ferns);
            conf = classifier.measure_forest(ferns);

            tmp.conf[i]=conf;
            tmp.patt[i]=ferns;
            if (conf>conf_pro)
            {
                dt.bb.push_back(i);
            }
        }
        else
        {
            tmp.conf[i]=0.0;
        }
    }

//    std::unique_lock<std::mutex> lk4_4(mut_det4_4);
//    data_cond_det4_4.wait(lk4_4, []{return isdetect4_4;});
//    isdetect4_4 = false;
//    lk4_4.unlock();

    int detections = dt.bb.size();

                                                                       //  Initialize detection structure
    dt.patt = vector<vector<int> >(detections,vector<int>(10,0));        //  Corresponding codes of the Ensemble Classifier
    dt.conf1 = vector<float>(detections);                                //  Relative Similarity (for final nearest neighbour classifier)
    dt.conf2 =vector<float>(detections);                                 //  Conservative Similarity (for integration with tracker)
    dt.isin = vector<vector<int> >(detections,vector<int>(3,-1));        //  Detected (isin=1) or rejected (isin=0) by nearest neighbour classifier
    dt.patch = vector<Mat>(detections,Mat(patch_size,patch_size,CV_32F/*CV_8U*/));//  Corresponding patches
    int idx;
    Scalar mean, stdev;
    float nn_th = classifier.getNNTh();

    if(detections > 0)
    {
        for (int i=0;i<detections;i++)
        {                                        //  for every remaining detection
            idx=dt.bb[i];                                                       //  Get the detected bounding box index
            patch = frame(grid[idx]);
            
            getPattern(patch,dt.patch[i],mean,stdev);                //  Get pattern within bounding box

            classifier.NNConf(dt.patch[i],dt.isin[i],dt.conf1[i],dt.conf2[i]);  //  Evaluate nearest neighbour classifier
//            dt.patt[i]=tmp.patt[idx];
            //DEBLOG("Testing feature %d, conf:%f isin:(%d|%d|%d)\n",i,dt.conf1[i],dt.isin[i][0],dt.isin[i][1],dt.isin[i][2]);

            if (dt.conf1[i]>nn_th)
            {                                               //  idx = dt.conf1 > tld.model.thr_nn; % get all indexes that made it through the nearest neighbour
                dbb.push_back(grid[idx]);                                           //  BB    = dt.bb(:,idx); % bounding boxes
                dconf.push_back(dt.conf2[i]);                                     //  Conf  = dt.conf2(:,idx); % conservative confidences
            }
        }
    }
    
    if (dbb.size()>0)
    {
         DEBLOG("xiangang->Found %d NN matches\n",(int)dbb.size());
        detected=true;
    }
    else
    {
         DEBLOG("No NN matches found.\n");
        detected=false;
    }

}

void evaluate()
{

}

void learn(const Mat& img)
{
  DEBLOG("[Learning] ");
  ///Check consistency
  BoundingBox bb;
  bb.x = max(lastbox.x,0);
  bb.y = max(lastbox.y,0);
  bb.width = min(min(img.cols-lastbox.x,lastbox.width),min(lastbox.width,lastbox.br().x));
  bb.height = min(min(img.rows-lastbox.y,lastbox.height),min(lastbox.height,lastbox.br().y));
  Scalar mean, stdev;
  Mat pattern = Mat::zeros(bb.height, bb.width, CV_8U);
  getPattern(img(bb),pattern,mean,stdev);
  vector<int> isin;
  float dummy, conf;
  classifier.NNConf(pattern,isin,conf,dummy);
  if (conf<0.5) {
      DEBLOG("Fast change..not training\n");
      lastvalid =false;
      return;
  }
  if ((stdev.val[0] * stdev.val[0])<var){
      DEBLOG("Low variance..not training\n");
      lastvalid=false;
      return;
  }
  if(isin[2]==1){
      DEBLOG("Patch in negative data..not traing");
      lastvalid=false;
      return;
  }
/// Data generation 重新计算重叠度
  for (int i=0;i<grid.size();i++){
      grid[i].overlap = bbOverlap(lastbox,grid[i]);
  }
  vector<pair<vector<int>,int> > fern_examples;
  good_boxes.clear();
  bad_boxes.clear();
  getOverlappingBoxes(lastbox,num_closest_update);
  if (good_boxes.size()>0)
    generatePositiveData(img,num_warps_update);
  else{
    lastvalid = false;
    DEBLOG("No good boxes..Not training");
    return;
  }
  fern_examples.reserve(pX.size()+bad_boxes.size());
  fern_examples.assign(pX.begin(),pX.end());
  int idx;
  for (int i=0;i<bad_boxes.size();i++){
      idx=bad_boxes[i];
      if (tmp.conf[idx]>=1){
          fern_examples.push_back(make_pair(tmp.patt[idx],0));
      }
  }
  vector<Mat> nn_examples;
  nn_examples.reserve(dt.bb.size()+1);
  nn_examples.push_back(pEx);
  for (int i=0;i<dt.bb.size();i++){
      idx = dt.bb[i];
      if (bbOverlap(lastbox,grid[idx]) < bad_overlap)
        nn_examples.push_back(dt.patch[i]);
  }
  /// Classifiers update
  classifier.trainF(fern_examples,2);
  classifier.trainNN(nn_examples);
//  classifier.show();
}

void buildGrid(const cv::Mat& img, const cv::Rect& box)
{
    const float SHIFT = 0.1;
    const float SCALES[] = {0.16151,0.19381,0.23257,0.27908,0.33490,0.40188,0.48225,
                          0.57870,0.69444,0.83333,1,1.20000,1.44000,1.72800,
                          2.07360,2.48832,2.98598,3.58318,4.29982,5.15978,6.19174};
    int width, height, min_bb_side;
    //Rect bbox;
    BoundingBox bbox;
    Size scale;
    int sc=0;
    for (int s=10;s<11;s++)
    {
        width = round(box.width*SCALES[s]);
        height = round(box.height*SCALES[s]);
        min_bb_side = min(height,width);
        if (min_bb_side < min_win || width > img.cols || height > img.rows)
            continue;
        scale.width = width;
        scale.height = height;
        scales.push_back(scale);
        for (int y=1;y<img.rows-height;y+=round(SHIFT*min_bb_side))
        {
            for (int x=1;x<img.cols-width;x+=round(SHIFT*min_bb_side))
            {
                bbox.x = x;
                bbox.y = y;
                bbox.width = width;
                bbox.height = height;
                bbox.overlap = bbOverlap(bbox,BoundingBox(box));
                bbox.sidx = sc;
                // DEBLOG("bbox:%d %d %d %d",bbox.x, bbox.y, bbox.width, bbox.height);
                grid.push_back(bbox);
            }
        }
        sc++;
    }
}

float bbOverlap(const BoundingBox& box1,const BoundingBox& box2){
  if (box1.x > box2.x+box2.width) { return 0.0; }
  if (box1.y > box2.y+box2.height) { return 0.0; }
  if (box1.x+box1.width < box2.x) { return 0.0; }
  if (box1.y+box1.height < box2.y) { return 0.0; }

  float colInt =  min(box1.x+box1.width,box2.x+box2.width) - max(box1.x, box2.x);
  float rowInt =  min(box1.y+box1.height,box2.y+box2.height) - max(box1.y,box2.y);

  float intersection = colInt * rowInt;
  float area1 = box1.width*box1.height;
  float area2 = box2.width*box2.height;
  return intersection / (area1 + area2 - intersection);
}

void getOverlappingBoxes(const cv::Rect& box1,int num_closest){
  float max_overlap = 0;
  for (int i=0;i<grid.size();i++){
      if (grid[i].overlap > max_overlap) {
          max_overlap = grid[i].overlap;
          best_box = grid[i];
      }

      if (grid[i].overlap > 0.6)
      {
          good_boxes.push_back(i);
      }
      else if (grid[i].overlap < bad_overlap){
          bad_boxes.push_back(i);
      }
  }
  //Get the best num_closest (10) boxes and puts them in good_boxes
  if (good_boxes.size()>num_closest){
    std::nth_element(good_boxes.begin(),good_boxes.begin()+num_closest,good_boxes.end(),OComparator(grid));
    good_boxes.resize(num_closest);
  }
  getBBHull();
}

void getBBHull(){
  int x1=INT_MAX, x2=0;
  int y1=INT_MAX, y2=0;
  int idx;
  for (int i=0;i<good_boxes.size();i++){
      idx= good_boxes[i];
      x1=min(grid[idx].x,x1);
      y1=min(grid[idx].y,y1);
      x2=max(grid[idx].x+grid[idx].width,x2);
      y2=max(grid[idx].y+grid[idx].height,y2);
  }
  bbhull.x = x1;
  bbhull.y = y1;
  bbhull.width = x2-x1;
  bbhull.height = y2 -y1;
}

bool bbcomp(const BoundingBox& b1,const BoundingBox& b2)
{
  // TLD t;
    if (bbOverlap(b1,b2)<0.5)
      return false;
    else
      return true;
}
int clusterBB(const vector<BoundingBox>& dbb,vector<int>& indexes){
  //FIXME: Conditional jump or move depends on uninitialised value(s)
  const int c = dbb.size();
  //1. Build proximity matrix
  Mat D(c,c,CV_32F);
  float d;
  for (int i=0;i<c;i++){
      for (int j=i+1;j<c;j++){
        d = 1-bbOverlap(dbb[i],dbb[j]);
        D.at<float>(i,j) = d;
        D.at<float>(j,i) = d;
      }
  }
  //2. Initialize disjoint clustering
 float L[c-1]; //Level
 int nodes[c-1][2];
 int belongs[c];
 int m=c;
 for (int i=0;i<c;i++){
    belongs[i]=i;
 }
 for (int it=0;it<c-1;it++){
 //3. Find nearest neighbor
     float min_d = 1;
     int node_a, node_b;
     for (int i=0;i<D.rows;i++){
         for (int j=i+1;j<D.cols;j++){
             if (D.at<float>(i,j)<min_d && belongs[i]!=belongs[j]){
                 min_d = D.at<float>(i,j);
                 node_a = i;
                 node_b = j;
             }
         }
     }
     if (min_d>0.5){
         int max_idx =0;
         bool visited;
         for (int j=0;j<c;j++){
             visited = false;
             for(int i=0;i<2*c-1;i++){
                 if (belongs[j]==i){
                     indexes[j]=max_idx;
                     visited = true;
                 }
             }
             if (visited)
               max_idx++;
         }
         return max_idx;
     }

 //4. Merge clusters and assign level
     L[m]=min_d;
     nodes[it][0] = belongs[node_a];
     nodes[it][1] = belongs[node_b];
     for (int k=0;k<c;k++){
         if (belongs[k]==belongs[node_a] || belongs[k]==belongs[node_b])
           belongs[k]=m;
     }
     m++;
 }
 return 1;

}

void clusterConf(const vector<BoundingBox>& dbb,const vector<float>& dconf,vector<BoundingBox>& cbb,vector<float>& cconf){
  int numbb =dbb.size();
  vector<int> T;
  float space_thr = 0.5;
  int c=1;
  switch (numbb){
  case 1:
    cbb=vector<BoundingBox>(1,dbb[0]);
    cconf=vector<float>(1,dconf[0]);
    return;
    break;
  case 2:
    T =vector<int>(2,0);
    if (1-bbOverlap(dbb[0],dbb[1])>space_thr){
      T[1]=1;
      c=2;
    }
    break;
  default:
    T = vector<int>(numbb,0);
    c = partition(dbb,T,(*bbcomp));
    //c = clusterBB(dbb,T);
    break;
  }
  cconf=vector<float>(c);
  cbb=vector<BoundingBox>(c);
  DEBLOG("Cluster indexes: ");
  BoundingBox bx;
  for (int i=0;i<c;i++){
      float cnf=0;
      int N=0,mx=0,my=0,mw=0,mh=0;
      for (int j=0;j<T.size();j++){
          if (T[j]==i){
              DEBLOG("%d ",i);
              cnf=cnf+dconf[j];
              mx=mx+dbb[j].x;
              my=my+dbb[j].y;
              mw=mw+dbb[j].width;
              mh=mh+dbb[j].height;
              N++;
          }
      }
      if (N>0){
          cconf[i]=cnf/N;
          bx.x=cvRound(mx/N);
          bx.y=cvRound(my/N);
          bx.width=cvRound(mw/N);
          bx.height=cvRound(mh/N);
          cbb[i]=bx;
      }
  }
  DEBLOG("\n");
}

