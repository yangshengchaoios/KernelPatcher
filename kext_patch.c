/*
 * Copyright (c) 2014 xZenue LLC. All rights reserved.
 *
 *
 * This work is licensed under the
 *  Creative Commons Attribution-NonCommercial 3.0 Unported License.
 *  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 */

#include "kernel_patcher.h"
#include "modules.h"
#include "libsaio.h"
#include <xml.h>
#include <stdio.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ "[" TOSTRING(__LINE__) "]: "

void handle_kext_entry(void* kernelData, TagPtr kextEntry)
{
    static section_t* txt;
    if(!txt) txt = lookup_section("__PRELINK_TEXT","__text");
    if(!txt) return;
    

    unsigned int kextSize = XMLCastInteger(XMLGetProperty(kextEntry, "_PrelinkExecutableSize"));
    if(kextSize)
    {
        UInt8* kernel = (UInt8*)kernelData;
        UInt32 kextStart = txt->offset;
        UInt32 kextOffset = txt->address;
        UInt32 kextBinAddress = XMLCastInteger(XMLGetProperty(kextEntry, "_PrelinkExecutableSourceAddr"));
        kextBinAddress -= kextOffset;
        kextBinAddress += kextStart; // calculate location in binary
        
        const char* kextName = XMLCastString(XMLGetProperty(kextEntry, "CFBundleExecutable"));
        
        //const char* kextPath = XMLCastString(XMLGetProperty(kextEntry, "_PrelinkBundlePath"));
        //unsigned int kextFinalAddress = XMLCastInteger(XMLGetProperty(kextEntry, "_PrelinkExecutableLoadAddr"));

        //printf("txt address 0x%0.8X offset 0x%0.8X\n", kextStart, kextOffset);
        //printf("Found kext %s (%s) %d bytes at 0x%0.8X\n", kextName, kextPath, kextSize, kextBinAddress);
        
        //printf("Found kext %s (%s) %d bytes at PRELINK_KEXT offset 0x%0.8X\n", kextName, kextPath, kextSize, kextBinAddress);
        
        // Notify the kext patcher that a kext has been located.
        //UInt8 kextOrigSize = kextSize;
        execute_hook("LoadMatchedModules", (void*)kextName, &kextSize, &kernel[kextBinAddress], NULL);
        
        // Todo: verify updated kext does not overwrite next kext.

    }
}

void patch_kexts(void* kernelData)
{
    UInt8* bytes = (UInt8*)kernelData;

	section_t* txt = lookup_section("__PRELINK_TEXT","__text");
    section_t* info = lookup_section("__PRELINK_INFO","__info");

	if(!txt)
	{
		printf(HEADER "Unable to locate __PRELINK_TEXT,__text\n");
		return;
	}

    if(!info)
	{
		printf(HEADER "Unable to locate __PRELINK_INFO,__info\n");
		return;
	}
    
    //UInt32 prelinked_text = txt->offset; // locaiton of txt in file
    UInt32 prelinked_info = info->offset; // location of plist in file
    //printf("Info at 0x%0.8X\n", prelinked_info);
    //printf("Kexts at 0x%0.8X\n", prelinked_text);
    
    TagPtr prelinkInfo = NULL;
    XMLParseFile((char*)&bytes[prelinked_info], &prelinkInfo);
    if(prelinkInfo)
    {
        if(XMLCastDict(prelinkInfo))
        {
            int numelements = XMLTagCount(prelinkInfo);
            int i;

            for(i = 0; i < numelements; i++)
            {
                TagPtr key = XMLGetKey(prelinkInfo, i);
                char* name = NULL;
                if(key) name = XMLCastString(key);

                if(strcmp(name, "_PrelinkInfoDictionary") == 0)
                {
                    TagPtr info = XMLGetProperty(prelinkInfo, name);
                    if(XMLCastArray(info))
                    {
                        int numkexts = XMLTagCount(info);
                        
                        verbose(HEADER "Notifying KextPatcher of %d potential kexts.\n", numkexts);

                        for(int j = 0; j < numkexts; j++)
                        {
                            TagPtr kextEntry = XMLGetElement(info, j);
                            if(XMLCastDict(kextEntry))
                            {
                                handle_kext_entry(kernelData, kextEntry);
                            }
                        }
                    }
                }
            }
        }
    }
}