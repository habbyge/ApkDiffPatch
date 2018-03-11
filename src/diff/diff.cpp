//  diff.cpp
//  ZipDiff
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
#include "diff.h"
#include <iostream>
#include <stdio.h>
#include "../../HDiffPatch/libHDiffPatch/HDiff/diff.h"  //https://github.com/sisong/HDiffPatch
#include "../../HDiffPatch/libHDiffPatch/HPatch/patch.h"
#include "../../HDiffPatch/file_for_patch.h"
#include "../../HDiffPatch/_clock_for_demo.h"
#include "../patch/OldStream.h"
#include "../patch/patch_types.h"
#include "../patch/patch.h"
#include "../../HDiffPatch/compress_plugin_demo.h"
#include "DiffData.h"

bool checkZipInfo(UnZipper* oldZip,UnZipper* newZip);
bool HDiffZ(const std::vector<TByte>& oldData,const std::vector<TByte>& newData,std::vector<TByte>& out_diffData,
            hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,int myBestMatchScore);
bool testZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath);
bool checkZipIsSame(const char* oldZipPath,const char* newZipPath,bool byteByByteCheckSame);

#define  check(value) { \
    if (!(value)){ printf(#value" ERROR!\n");  \
        result=false; if (!_isInClear){ goto clear; } } }


bool ZipDiff(const char* oldZipPath,const char* newZipPath,const char* outDiffFileName,const char* temp_ZipPatchFileName){
    const int           myBestMatchScore=3;
    UnZipper            oldZip;
    UnZipper            newZip;
    TFileStreamOutput   out_diffFile;
    std::vector<TByte>  newData;
    std::vector<TByte>  oldData;
    std::vector<TByte>  hdiffzData;
    std::vector<TByte>  out_diffData;
    std::vector<uint32_t> samePairList;
    std::vector<uint32_t> newRefList;
    std::vector<uint32_t> newRefNotDecompressList;
    std::vector<uint32_t> newRefCompressedSizeList;
    std::vector<uint32_t> oldRefList;
    std::vector<uint32_t> oldRefNotDecompressList;
    bool            result=true;
    bool            _isInClear=false;
    bool            byteByByteCheckSame=false;
    int             oldZipFileCount=0;
    size_t          newZipAlignSize=0;
#ifdef _CompressPlugin_zstd
    zstd_compress_level=22; //0..22
    hdiff_TCompress* compressPlugin=&zstdCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&zstdDecompressPlugin;
#else
    zlib_compress_level=9; //0..9
    hdiff_TCompress* compressPlugin=&zlibCompressPlugin;
    hpatch_TDecompress* decompressPlugin=&zlibDecompressPlugin;
#endif
    UnZipper_init(&oldZip);
    UnZipper_init(&newZip);
    TFileStreamOutput_init(&out_diffFile);
    
    check(UnZipper_openRead(&oldZip,oldZipPath));
    check(UnZipper_openRead(&newZip,newZipPath));
    oldZip._isNormalized=getZipCompressedDataIsNormalized(&oldZip);
    //todo: && 2个文件储存位置不矛盾！
    newZip._isNormalized=getZipCompressedDataIsNormalized(&newZip);
    newZipAlignSize=getZipAlignSize_unsafe(&newZip);
    if (UnZipper_isHaveApkV2Sign(&newZip))
        newZip._isNormalized&=(newZipAlignSize>0);//precondition (+checkZipIsSame() to complete)
    newZipAlignSize=(newZipAlignSize>0)?newZipAlignSize:kDefaultZipAlignSize;
    byteByByteCheckSame=UnZipper_isHaveApkV2Sign(&newZip);
    check(checkZipInfo(&oldZip,&newZip));
    
    std::cout<<"ZipDiff with compress plugin: \""<<compressPlugin->compressType(compressPlugin)<<"\"\n";

    check(getSamePairList(&newZip,&oldZip,samePairList,newRefList,newRefNotDecompressList,newRefCompressedSizeList));
    
    //todo: get minSize best oldZip refList
    oldZipFileCount=UnZipper_fileCount(&oldZip);
    for (int i=0; i<oldZipFileCount; ++i) {
        if (UnZipper_file_isApkV2Compressed(&oldZip,i))
            oldRefNotDecompressList.push_back(i);
        else
            oldRefList.push_back(i);
    }
    std::cout<<"ZipDiff same file count: "<<samePairList.size()/2<<"\n";
    std::cout<<"    diff new file count: "<<newRefList.size()+newRefNotDecompressList.size()<<"\n";
    std::cout<<"     ref old file count: "<<oldRefList.size()+oldRefNotDecompressList.size()<<" ("<<oldZipFileCount<<")\n";
    std::cout<<"     ref old decompress: "
        <<OldStream_getDecompressSumSize(&oldZip,oldRefList.data(),oldRefList.size()) <<" byte\n";
    //for (int i=0; i<(int)newRefList.size(); ++i) std::cout<<zipFile_name(&newZip,newRefList[i])<<"\n";
    //for (int i=0; i<(int)newRefNotDecompressList.size(); ++i) std::cout<<zipFile_name(&newZip,newRefNotDecompressList[i])<<"\n";

    check(readZipStreamData(&newZip,newRefList,newRefNotDecompressList,newData));
    check(readZipStreamData(&oldZip,oldRefList,oldRefNotDecompressList,oldData));
    check(HDiffZ(oldData,newData,hdiffzData,compressPlugin,decompressPlugin,myBestMatchScore));
    { std::vector<TByte> _empty; oldData.swap(_empty); }
    { std::vector<TByte> _empty; newData.swap(_empty); }
    
    check(serializeZipDiffData(out_diffData,&newZip,&oldZip,newZipAlignSize,
                               samePairList,newRefNotDecompressList,newRefCompressedSizeList,
                               oldRefList,oldRefNotDecompressList,hdiffzData,compressPlugin));
    std::cout<<"\nZipDiff size: "<<out_diffData.size()<<"\n";

    check(TFileStreamOutput_open(&out_diffFile,outDiffFileName,out_diffData.size()));
    check(out_diffData.size()==out_diffFile.base.write(out_diffFile.base.streamHandle,
                                                       0,out_diffData.data(),out_diffData.data()+out_diffData.size()));
    check(TFileStreamOutput_close(&out_diffFile));
    std::cout<<"  out ZipDiff file ok!\n";
    check(UnZipper_close(&newZip));
    check(UnZipper_close(&oldZip));
    
    check(testZipPatch(oldZipPath,outDiffFileName,temp_ZipPatchFileName));
    check(checkZipIsSame(newZipPath,temp_ZipPatchFileName,byteByByteCheckSame));
    
clear:
    _isInClear=true;
    check(TFileStreamOutput_close(&out_diffFile));
    check(UnZipper_close(&newZip));
    check(UnZipper_close(&oldZip));
    return result;
}

bool checkZipInfo(UnZipper* oldZip,UnZipper* newZip){
    if (oldZip->_isNormalized)
        printf("  NOTE: oldZip Normalized\n");
    if (UnZipper_isHaveApkV1_or_jarSign(oldZip))
        printf("  NOTE: oldZip found ApkV1Sign or JarSign\n");
    if (UnZipper_isHaveApkV2Sign(oldZip))
        printf("  NOTE: oldZip found ApkV2Sign\n");
    if (newZip->_isNormalized)
        printf("  NOTE: newZip Normalized\n");
    if (UnZipper_isHaveApkV1_or_jarSign(newZip))
        printf("  NOTE: newZip found ApkV1Sign or JarSign\n");
    bool newIsV2Sign=UnZipper_isHaveApkV2Sign(newZip);
    if (newIsV2Sign)
        printf("  NOTE: newZip found ApkV2Sign\n");
    
    if (newIsV2Sign&(!newZip->_isNormalized)){
        printf("  ERROR: newZip not Normalized, need run ApkNormalized(newZip) before run ZipDiff!\n");
        return false;
    }
    return true;
}

bool HDiffZ(const std::vector<TByte>& oldData,const std::vector<TByte>& newData,std::vector<TByte>& out_diffData,
            hdiff_TCompress* compressPlugin,hpatch_TDecompress* decompressPlugin,int myBestMatchScore){
    double time0=clock_s();
    const size_t oldDataSize=oldData.size();
    const size_t newDataSize=newData.size();
    std::cout<<"\nrun HDiffZ:\n";
    std::cout<<"  oldDataSize : "<<oldDataSize<<"\n  newDataSize : "<<newDataSize<<"\n";
    
    std::vector<TByte>& diffData=out_diffData;
    const TByte* newData0=newData.data();
    const TByte* oldData0=oldData.data();
    create_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                           diffData,compressPlugin,myBestMatchScore);
    double time1=clock_s();
    std::cout<<"  diffDataSize: "<<diffData.size()<<"\n";
    std::cout<<"  diff  time: "<<(time1-time0)<<" s\n";
    if (!check_compressed_diff(newData0,newData0+newDataSize,oldData0,oldData0+oldDataSize,
                               diffData.data(),diffData.data()+diffData.size(),decompressPlugin)){
        std::cout<<"\n  HPatch check HDiffZ result error!!!\n";
        return false;
    }else{
        double time2=clock_s();
        std::cout<<"  HPatch check HDiffZ result ok!\n";
        std::cout<<"  patch time: "<<(time2-time1)<<" s\n";
        return true;
    }
}


bool testZipPatch(const char* oldZipPath,const char* zipDiffPath,const char* outNewZipPath){
    double time0=clock_s();
    TPatchResult ret=ZipPatch(oldZipPath,zipDiffPath,outNewZipPath,0,0);
    double time1=clock_s();
    if (ret==PATCH_SUCCESS){
        std::cout<<"\nrun ZipPatch ok!\n";
        std::cout<<"  patch time: "<<(time1-time0)<<" s\n";
        return true;
    }else{
        return false;
    }
}


static bool getFileIsSame(const char* xFileName,const char* yFileName){
    TFileStreamInput x;
    TFileStreamInput y;
    bool            result=true;
    bool            _isInClear=false;
    std::vector<TByte> buf;
    size_t          fileSize;
    TFileStreamInput_init(&x);
    TFileStreamInput_init(&y);
    check(TFileStreamInput_open(&x,xFileName));
    check(TFileStreamInput_open(&y,yFileName));
    fileSize=(size_t)x.base.streamSize;
    assert(fileSize==x.base.streamSize);
    check(fileSize==y.base.streamSize);
    if (fileSize>0){
        buf.resize(fileSize*2);
        check(fileSize==x.base.read(x.base.streamHandle,0,buf.data(),buf.data()+fileSize));
        check(fileSize==y.base.read(y.base.streamHandle,0,buf.data()+fileSize,buf.data()+fileSize*2));
        check(0==memcmp(buf.data(),buf.data()+fileSize,fileSize));
    }
clear:
    _isInClear=true;
    TFileStreamInput_close(&x);
    TFileStreamInput_close(&y);
    return result;
}

bool checkZipIsSame(const char* oldZipPath,const char* newZipPath,bool byteByByteCheckSame){
    double time0=clock_s();
    bool result;
    if (byteByByteCheckSame)
        result=getFileIsSame(oldZipPath,newZipPath);
    else
        result=getZipIsSame(oldZipPath,newZipPath);
    double time1=clock_s();
    if (result){
        std::cout<<"  check ZipPatch result ok!\n";
        std::cout<<"  check time: "<<(time1-time0)<<" s\n";
    }
    return result;
}
