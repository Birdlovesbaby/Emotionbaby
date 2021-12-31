//코드 출처: 마이크로프로세서응용실습 교안

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <time.h>
#include <assert.h>

//OpenCL 관련
#include <CL/opencl.h>
#include "ImageProcessing.h"
#define CL_FILE "/data/local/tmp/GrayScale.cl"

// 디버깅 관련
#define LOG_TAG "DEBUG"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define CHECK_CL(err) {\
    cl_int er = (err);\
    if(er<0 && er > -64){\
        LOGE("%d line, OpenCL Error:%d\n",__LINE__,er);\
    }\
}


JNIEXPORT jobject JNICALL
Java_com_example_dontcrybaby_MainActivity_GrayScale
        (JNIEnv *env, jclass class, jobject bitmap )
{
    LOGD("reading bitmap info...");
    AndroidBitmapInfo info;
    int ret;
    if ((ret = AndroidBitmap_getInfo(env, bitmap, &info)) < 0) {
        LOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return NULL;
    }
    LOGD("width:%d height:%d stride:%d", info.width, info.height, info.stride);
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("Bitmap format is not RGBA_8888!");
        return NULL;
    }


    LOGD("reading bitmap pixels...");
    void* bitmapPixels;
    if ((ret = AndroidBitmap_lockPixels(env, bitmap, &bitmapPixels)) < 0) {
        LOGE("AndroidBitmap_lockPixels() failed ! error=%d", ret);
        return NULL;
    }
    uint32_t* src = (uint32_t*) bitmapPixels;
    uint32_t* tempPixels = (uint32_t*)malloc(info.height * info.width*4);
    uint32_t pixelsCount = info.height * info.width;
    memcpy(tempPixels, src, sizeof(uint32_t) * pixelsCount);

    LOGD("GPU Start");
    FILE* file_handle;
    char* kernel_file_buffer, * file_log;
    size_t kernel_file_size, log_size;

    unsigned char* cl_file_name = CL_FILE;
    unsigned char* kernel_name = "kernel_gray";

    cl_mem d_src;
    cl_mem d_dst;

    cl_platform_id clPlatform;
    cl_device_id device_id;
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    LOGD("cl_file_open");
    file_handle = fopen(cl_file_name, "r");
    if (file_handle == NULL) {
        printf("Couldn't find the file");
        exit(1);
    }

    //read kernel file
    fseek(file_handle, 0, SEEK_END);
    kernel_file_size = ftell(file_handle);
    rewind(file_handle);
    kernel_file_buffer = (char*)malloc(kernel_file_size + 1);
    kernel_file_buffer[kernel_file_size] = '\0';
    fread(kernel_file_buffer, sizeof(char), kernel_file_size, file_handle);
    fclose(file_handle);
    LOGD("%s",kernel_file_buffer);
    LOGD("file_buffer_read");
    // Initialize vectors on host
    int i;

    size_t globalSize, localSize, grid;
    cl_int err;


    localSize = 64;
    int n_pix = info.width * info.height;


    grid = (n_pix % localSize) ? (n_pix / localSize) + 1 : n_pix / localSize;
    globalSize = grid * localSize;

    LOGD("calc grid and globalSize");

    LOGD("error check");
    CHECK_CL(clGetPlatformIDs(1, &clPlatform, NULL))
    LOGD("error check");

    CHECK_CL(clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_GPU, 1, &device_id, NULL))

    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    CHECK_CL(err)

    queue = clCreateCommandQueue(context, device_id, 0, &err);
    CHECK_CL(err)

    program = clCreateProgramWithSource(context, 1, (const char**)&kernel_file_buffer, &kernel_file_size, &err);
    CHECK_CL(err)

    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    if (err != CL_SUCCESS) {
        LOGD("%s", err);
        size_t len;
        char buffer[4096];
        LOGD("Error: Failed to build program executable!");
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer),
                              buffer, &len);

        LOGD("%s", buffer);
    }
    LOGD("error check!");


    kernel = clCreateKernel(program, kernel_name, &err);
    CHECK_CL(err)


    d_src = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(uint32_t)*info.width*info.height, NULL, NULL);
    d_dst = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t)*info.width*info.height, NULL, NULL);


    CHECK_CL(clEnqueueWriteBuffer(queue, d_src, CL_TRUE, 0, sizeof(uint32_t)*info.width*info.height, tempPixels, 0, NULL, NULL))


    CHECK_CL(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_src))
    CHECK_CL(clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_dst))
    CHECK_CL(clSetKernelArg(kernel, 2, sizeof(uint32_t), &info.width))
    CHECK_CL(clSetKernelArg(kernel, 3, sizeof(uint32_t), &info.height))



    CHECK_CL(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalSize, &localSize, 0, NULL, NULL))

    CHECK_CL(clFinish(queue))

    CHECK_CL(clEnqueueReadBuffer(queue, d_dst, CL_TRUE, 0, sizeof(uint32_t)*info.width*info.height, src, 0, NULL, NULL))



    clReleaseMemObject(d_src);
    clReleaseMemObject(d_dst);
    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    AndroidBitmap_unlockPixels(env, bitmap);

    free(tempPixels);
    return bitmap;
}


