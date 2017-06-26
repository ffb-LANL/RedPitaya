#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "util.h"
#include "uio.h"
#include "evn.h"
#include "gen.h"
#include "asg_per.h"
#include "asg_bst.h"
#include "gen_out.h"

int rp_gen_init (rp_gen_t *handle, const int unsigned index) {
    static char path_gen [] = "/dev/uio/gen";
    size_t len = strlen(path_gen)+1+1;
    char path[len];

    // initialize constants
    handle->FS = 125000000.0;  // sampling frequency
    handle->buffer_size = 1<<14;
    handle->dat_t = (fixp_t) {.s = 1, .m = 0, .f = DW-1};

    // a single character is reserved for the index
    // so indexes above 9 are an error
    if (index>9) {
        fprintf(stderr, "GEN: failed device index decimal representation is limited to 1 digit.\n");
        return (-1);
    }
    snprintf(path, len, "%s%u", path_gen, index);
    
    int status = rp_uio_init (&handle->uio, path);
    if (status) {
        return (-1);
    }
    // map regset
    handle->regset = (rp_gen_regset_t *) handle->uio.map[0].mem;
    // map table
    handle->table = (int16_t *) handle->uio.map[0].mem;

    // events
    rp_evn_init(&handle->evn, &handle->regset->evn);
    // continuous/periodic mode
    const fixp_t cnt_t = {.s = 0, .m = CWM, .f = CWF};
    rp_asg_per_init(&handle->per, &handle->regset->per, handle->FS, handle->buffer_size, cnt_t);
    // burst mode
    const fixp_t bdr_t = {.s = 0, .m = CWR, .f = 0};
    const fixp_t bpl_t = {.s = 0, .m = CWL, .f = 0};
    const fixp_t bpn_t = {.s = 0, .m = CWN, .f = 0};
    rp_asg_bst_init(&handle->bst, &handle->regset->bst, handle->FS, handle->buffer_size, bdr_t, bpl_t, bpn_t);
    // output configuration
    const fixp_t mul_t = {.s = 1, .m = 1, .f = DWM-2};
    const fixp_t sum_t = {.s = 1, .m = 0, .f = DWS-1};
    rp_gen_out_init(&handle->out, &handle->regset->out, mul_t, sum_t);

    return(0);
}

int rp_gen_release (rp_gen_t *handle) {
    // disable output
    rp_gen_out_set_enable(&handle->out, false);
    // reset hardware
    rp_evn_reset(&handle->evn);

    int status = rp_uio_release (&handle->uio);
    if (status) {
        return (-1);
    }

    return(0);
}

static inline int unsigned rp_gen_get_length(rp_gen_t *handle) {
    if (rp_gen_get_mode(handle) == CONTINUOUS) {
        return(rp_asg_per_get_table_size(&handle->per));
    } else {
        return(rp_asg_bst_get_data_length(&handle->bst));
    }
    return(0);
}

int rp_gen_get_waveform(rp_gen_t *handle, float *waveform, int unsigned *len) {
    if (len == 0) {
        *len = rp_gen_get_length(handle);
    }
    if (*len <= handle->buffer_size) {
        waveform = malloc(*len * sizeof(float));
        for (int unsigned i=0; i<*len; i++) {
            waveform [i] = (float) handle->table [i] / (float) fixp_unit(handle->dat_t);
        }
        return(0);
    } else {
        return(-1);
    }
}

int rp_gen_set_waveform(rp_gen_t *handle, float *waveform, int unsigned len) {
    if (len <= handle->buffer_size) {
        for (int unsigned i=0; i<len; i++) {
            handle->table [i] = (uint16_t) (waveform [i] * (float) fixp_unit(handle->dat_t));
        }
        return(0);
    } else {
        return(-1);
    }
}

rp_gen_mode_t rp_gen_get_mode(rp_gen_t *handle) {
    return((rp_gen_mode_t) handle->regset->cfg_bmd);
}

void rp_gen_set_mode(rp_gen_t *handle, rp_gen_mode_t value) {
    handle->regset->cfg_bmd = (uint32_t) value;
}
