#include "Rf.h"
#include <chrono>
#include <iostream>
int main(){RfProcessor receiver;RfParams p;p.enabled=true;p.contact=.4f;p.noise=.1f;std::array<uint32_t,256*240> image;for(size_t i=0;i<image.size();++i)image[i]=0xff000000u|uint32_t(i*12345);double checksum=0;auto start=std::chrono::steady_clock::now();for(int n=0;n<180;++n){std::vector<float> a(800,.12f);receiver.Process(image.data(),a,60.0988,p);checksum+=a[n%800];}std::cout<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/180<<" ms/frame, check="<<checksum<<'\n';}
