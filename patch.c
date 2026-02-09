/*
 * patch.c
 *
 * copyright (C) 2022/12/04 dora2ios
 * copyright (C) 2026/02/08 crystall1nedev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

#include "offsetfinder.h"

#define LOG(x, ...) \
do { \
printf("[LOG] "x"\n", ##__VA_ARGS__); \
} while(0)

#define ERR(x, ...) \
do { \
printf("[ERR] "x"\n", ##__VA_ARGS__); \
} while(0)


#ifdef DEVBUILD
#define DEVLOG(x, ...) \
do { \
printf("[DEV] "x"\n", ##__VA_ARGS__); \
} while(0)
#else
#define DEVLOG(x, ...)
#endif

#define INSN_MOV_X0_0           0xd2800000
#define INSN_MOV_X0_1           0xd2800020
#define INSN_RET                0xd65f03c0
#define INSN_NOP                0xd503201f

int open_file(char *file, size_t *sz, unsigned char **buf)
{
    FILE *fd = fopen(file, "r");
    if (!fd) {
        ERR("error opening %s", file);
        return -1;
    }
    
    fseek(fd, 0, SEEK_END);
    *sz = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    *buf = malloc(*sz);
    if (!*buf) {
        ERR("error allocating file buffer");
        fclose(fd);
        return -1;
    }
    
    fread(*buf, *sz, 1, fd);
    fclose(fd);
    
    return 0;
}

#define SUB (0x000100000)

void usage(const char *path)
{
    printf("%s [-RF] -i <in> -o <out>\n", path);
    printf("Version: " VERSION "\n");
}

int main(int argc, char **argv)
{
    int option;
    char *infile = NULL;
    char *outfile = NULL;
    int replaceSys = 0;
    int enableFilenames = 0;

    while ((option = getopt(argc, argv, "i:o:RF")) != -1) {
        switch (option) {
            // -i = input file
            // -o = output file
            // -R = "replace system", enable isys patch
            // -F = "filenames", enable filename patching
            case 'i': infile = optarg; break;
            case 'o': outfile = optarg; break;
            case 'R': replaceSys = 1; break;
            case 'F': enableFilenames = 1; break;
            default: break;
        }
    }
    
    unsigned char* idata;
    size_t isize;
    if(open_file(infile, &isize, &idata))
        return -1;
    assert(isize && idata);
    
    
    {
        uint64_t iboot_base = *(uint64_t*)(idata + 0x300);
        if(!iboot_base)
            goto end;
        LOG("%016" PRIx64 "[%016" PRIx64 "]: iboot_base", iboot_base, (uint64_t)0x300);
        
        uint64_t check_bootmode = find_check_bootmode(iboot_base, idata, isize);
        if(!check_bootmode) {
            ERR("Failed to find check_bootmode");
            goto end;
        }
        LOG("%016" PRIx64 "[%016" PRIx64 "]: check_bootmode", check_bootmode + iboot_base, check_bootmode);
        
        uint64_t check_system_volume_auth_blob = find_system_volume_auth_blob(iboot_base, idata, isize);
        
        uint64_t kc_str = find_kc(iboot_base, idata, isize);
        uint64_t dtre = find_dtre(iboot_base, idata, isize);
        uint64_t avef = find_avef(iboot_base, idata, isize);
        
        if(replaceSys) {
            
            if(!check_system_volume_auth_blob) {
                ERR("Failed to find system_volume_auth_blob");
                goto end;
            }
            LOG("%016" PRIx64 "[%016" PRIx64 "]: check_system_volume_auth_blob", check_system_volume_auth_blob + iboot_base, check_system_volume_auth_blob);
            
        }
        
        if(!replaceSys || enableFilenames) {
            if(!kc_str) {
                ERR("Failed to find kernelcache string");
                goto end;
            }
            LOG("%016" PRIx64 "[%016" PRIx64 "]: kc_str", kc_str + iboot_base, kc_str);
            if(!dtre) {
                ERR("Failed to find devicetree string");
                goto end;
            }
            LOG("%016" PRIx64 "[%016" PRIx64 "]: dtre", dtre + iboot_base, dtre);
            
            if(!dtre) {
                ERR("Failed to find AVE firmware string");
                goto end;
            }
            LOG("%016" PRIx64 "[%016" PRIx64 "]: avef", avef + iboot_base, avef);
            
        }

        /*---- patch part ----*/
        {
            uint32_t* patch_check_bootmode = (uint32_t*)(idata + check_bootmode);
            uint32_t opcode = INSN_MOV_X0_0;  // 0: LOCAL_BOOT, 1: REMOTE_BOOT
            if((opcode & 0xffffffdf) != 0xd2800000)
            {
                ERR("Detected weird opcode");
                goto end;
            }
            uint32_t bootmode = (opcode & 0xf0) >> 5;
            patch_check_bootmode[0] = opcode;
            patch_check_bootmode[1] = INSN_RET;
            LOG("set bootmode=%d (%s)", bootmode, bootmode == 0 ? "LOCAL_BOOT" : "REMOTE_BOOT");
        }
        
        if(replaceSys) {
            {
                uint32_t* patch_system_volume_auth_blob = (uint32_t*)(idata + check_system_volume_auth_blob);
                patch_system_volume_auth_blob[0] = INSN_NOP;
                patch_system_volume_auth_blob[1] = INSN_NOP;
                LOG("disabled system-volume-auth-blob verification");
            }
        }
        
        if(!replaceSys || enableFilenames) {
            {
                uint8_t* patch_kc_str = (uint8_t*)(idata + kc_str);
                patch_kc_str[0] = 'd';
                LOG("kernelcache -> kernelcachd");
            }

            {
                uint8_t* patch_avef_str = (uint8_t*)(idata + avef);
                patch_avef_str[0] = 'E';
                patch_avef_str[2] = 'A';
                LOG("AVE.img4 -> EVA.img4");
            }

            {
                uint8_t* patch_dtre_str = (uint8_t*)(idata + dtre);
                patch_dtre_str[34] = 'd';
                LOG("devicetree.img4 -> devicetred.img4");
            }
        }
    }
    
    
    
    FILE *out = fopen(outfile, "w");
    if (!out) {
        ERR("error opening %s", outfile);
        return -1;
    }
    
    LOG("writing %s...", outfile);
    fwrite(idata, isize, 1, out);
    fflush(out);
    fclose(out);
    
    
end:
    if(idata)
        free(idata);
    
    return 0;
}
