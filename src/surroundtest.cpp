#include "surround.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
int main(){
  int R=48000,N=R*2;
  auto test=[&](const char*name,int kind){
   for(int m=1;m<=9;m++){
    Surround s; s.configure((Listen)m,R);
    std::vector<float> in(N*2),out;
    for(int i=0;i<N;i++){ float x=0.3f*std::sin(2*3.14159f*440*i/R)+0.1f*std::sin(2*3.14159f*1500*i/R);
      float l=0,r=0;
      if(kind==0){l=x;r=x;}          // centre
      else if(kind==1){l=x;r=-x;}    // surround
      else if(kind==2){l=x;r=0;}     // gauche
      in[2*i]=l;in[2*i+1]=r;}
    s.process(in.data(),N,out);
    int oc=s.outChannels(); double e[8]={0};
    for(int i=N/2;i<N;i++)for(int c=0;c<oc;c++)e[c]+=out[i*oc+c]*out[i*oc+c];
    printf("%s mode%d oc=%d :",name,m,oc);
    for(int c=0;c<oc;c++)printf(" %5.1f",10*std::log10(e[c]/(N/2)+1e-12));
    printf("\n");
    Virtualizer v;v.configure(oc,R);std::vector<float>o2;v.process(out.data(),N,o2);
    for(float f:o2)if(f!=f){printf("NaN!\n");return;}
   }};
  test("centre   ",0);test("surround ",1);test("gauche   ",2);
}
