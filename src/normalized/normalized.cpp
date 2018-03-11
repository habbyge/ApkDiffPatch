//  normalized.cpp
//  ApkNormalized
/*
 The MIT License (MIT)
 Copyright (c) 2016-2018 HouSisong
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#include "normalized.h"
#include <vector>
#include "../patch/Zipper.h"
#include "../diff/DiffData.h"

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }

bool ZipNormalized(const char* srcApk,const char* dstApk,int ZipAlignSize){
    bool result=true;
    bool _isInClear=false;
    int  fileCount=0;
    bool isHaveApkV2Sign=false;
    int  copyCompressedCount=0;
    std::vector<int>   fileIndexs;
    UnZipper unzipper;
    Zipper   zipper;
    UnZipper_init(&unzipper);
    Zipper_init(&zipper);
    
    check(UnZipper_openRead(&unzipper,srcApk));
    fileCount=UnZipper_fileCount(&unzipper);
    check(Zipper_openWrite(&zipper,dstApk,fileCount,ZipAlignSize));
    
    //sort file
    for (int i=0; i<fileCount; ++i) {
        if (!UnZipper_file_isApkV1_or_jarSign(&unzipper,i))
            fileIndexs.push_back(i);
    }
    if ((int)fileIndexs.size()<fileCount)
        printf("NOTE: src found ApkV1Sign or JarSign(%d file)\n",fileCount-(int)fileIndexs.size());
    for (int i=0; i<fileCount; ++i) {
        if (UnZipper_file_isApkV1_or_jarSign(&unzipper,i))
            fileIndexs.push_back(i);
    }
    isHaveApkV2Sign=UnZipper_isHaveApkV2Sign(&unzipper);
    if (isHaveApkV2Sign)
        printf("NOTE: src found ApkV2Sign and not out(%d Byte)\n",(int)(unzipper._centralDirectory-unzipper._cache_vce));
    printf("src fileCount:%d\nout fileCount:%d\n\n",fileCount,(int)fileIndexs.size());

    
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        std::string fileName=zipFile_name(&unzipper,fileIndex);
        bool isCopyCompressed=UnZipper_file_isApkV2Compressed(&unzipper,fileIndex);
        printf("\"%s\"",fileName.c_str());
        if (isCopyCompressed){
            printf("     \t\t(Copy old Compressed %d)",copyCompressedCount);
            ++copyCompressedCount;
        }
        printf("\n");
        
        check(Zipper_file_append_copy(&zipper,&unzipper,fileIndex,!isCopyCompressed));
    }
    printf("\n");
    
    //no run: check(Zipper_copyApkV2Sign_before_fileHeader(&zipper,&unzipper));
    for (int i=0; i<(int)fileIndexs.size(); ++i) {
        int fileIndex=fileIndexs[i];
        check(Zipper_fileHeader_append(&zipper,&unzipper,fileIndex));
    }
    check(Zipper_endCentralDirectory_append(&zipper,&unzipper));
    
clear:
    _isInClear=true;
    check(UnZipper_close(&unzipper));
    check(Zipper_close(&zipper));
    return result;
}