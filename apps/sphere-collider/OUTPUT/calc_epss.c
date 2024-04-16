#include <stdio.h>
#include <stdlib.h>
#include <math.h>

const double from[] = {0,0,0};
const double to[] = {1,1,1};
const int res = 100;

double r=0.1;
double snapshots = 400;
double snap_stride = 2;

char * filename_template = "snap_%03d.csv";

char filename[1024];


int main(int argc, char *argv[])
{
	FILE * f;
	int snap;
	int sphere_count;
	int hits;
	double rsq = r*r;
		
	double pos[1000][3];
	
	for(snap=snap_stride;snap<=snapshots;snap+=snap_stride) {
		sprintf(filename, filename_template, snap);
		f = fopen(filename,"r");
		if(!f) {
			printf("Error opening file: %s\n", filename);
			exit(1);
		}
		sphere_count = 0;
		fscanf(f,"%*s\n");	// skip header
		while(!feof(f)) {
			fscanf(f,"%lf,%lf,%lf,%*s\n", pos[sphere_count], pos[sphere_count]+1, pos[sphere_count]+2);
			sphere_count++;
		}
		fclose(f);

		hits = 0;
		#pragma omp parallel reduction(+:hits)
		{
			double x,y,z;
			int i,j,k,s;
			#pragma omp for
			for(k=0;k<res;k++) {
				z = from[2] + (to[2]-from[2])*(0.5+k)/res;
				for(j=0;j<res;j++) {
					y = from[1] + (to[1]-from[1])*(0.5+j)/res;
					for(i=0;i<res;i++) {
						x = from[0] + (to[0]-from[0])*(0.5+i)/res;
						for(s=0;s<sphere_count;s++)
							if((x-pos[s][0])*(x-pos[s][0]) + (y-pos[s][1])*(y-pos[s][1]) + (z-pos[s][2])*(z-pos[s][2]) <= rsq) hits++;
					}	// i
				}	// j
			}	// k
		}	// parallel region
		
		printf("%lf\n", ((double)hits)/(res*res*res));
	}	// snap

	return(0);
} 
