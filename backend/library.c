/*  library.c - external functions of libzint

    libzint - the open source barcode library
    Copyright (C) 2009 - 2021 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* vim: set ts=4 sw=4 et : */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#ifdef _MSC_VER
#include <malloc.h>
#endif
#include "common.h"
#include "eci.h"
#include "gs1.h"
#include "zfiletypes.h"

#define TECHNETIUM  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%"

/* It's assumed that int is at least 32 bits, the following will compile-time fail if not
 * https://stackoverflow.com/a/1980056/664741 */
typedef int static_assert_int_at_least_32bits[CHAR_BIT != 8 || sizeof(int) < 4 ? -1 : 1];

struct zint_symbol *ZBarcode_Create() {
    struct zint_symbol *symbol;

    symbol = (struct zint_symbol *) malloc(sizeof(*symbol));
    if (!symbol) return NULL;

    memset(symbol, 0, sizeof(*symbol));

    symbol->symbology = BARCODE_CODE128;
    strcpy(symbol->fgcolour, "000000");
    symbol->fgcolor = &symbol->fgcolour[0];
    strcpy(symbol->bgcolour, "ffffff");
    symbol->bgcolor = &symbol->bgcolour[0];
#ifdef NO_PNG
    strcpy(symbol->outfile, "out.gif");
#else
    strcpy(symbol->outfile, "out.png");
#endif
    symbol->scale = 1.0f;
    symbol->option_1 = -1;
    symbol->show_hrt = 1; // Show human readable text
    symbol->fontsize = 8;
    symbol->input_mode = DATA_MODE;
    symbol->bitmap = NULL;
    symbol->alphamap = NULL;
    symbol->eci = 0; // Default 0 uses ECI 3
    symbol->dot_size = 4.0f / 5.0f;
    symbol->vector = NULL;
    symbol->warn_level = WARN_DEFAULT;

    return symbol;
}

INTERNAL void vector_free(struct zint_symbol *symbol); /* Free vector structures */

void ZBarcode_Clear(struct zint_symbol *symbol) {
    int i, j;

    if (!symbol) return;

    for (i = 0; i < symbol->rows; i++) {
        for (j = 0; j < symbol->width; j++) {
            unset_module(symbol, i, j);
        }
    }
    symbol->rows = 0;
    symbol->width = 0;
    memset(symbol->text, 0, sizeof(symbol->text));
    symbol->errtxt[0] = '\0';
    if (symbol->bitmap != NULL) {
        free(symbol->bitmap);
        symbol->bitmap = NULL;
    }
    if (symbol->alphamap != NULL) {
        free(symbol->alphamap);
        symbol->alphamap = NULL;
    }
    symbol->bitmap_width = 0;
    symbol->bitmap_height = 0;

    // If there is a rendered version, ensure its memory is released
    vector_free(symbol);
}

void ZBarcode_Delete(struct zint_symbol *symbol) {
    if (!symbol) return;

    if (symbol->bitmap != NULL)
        free(symbol->bitmap);
    if (symbol->alphamap != NULL)
        free(symbol->alphamap);

    // If there is a rendered version, ensure its memory is released
    vector_free(symbol);

    free(symbol);
}

INTERNAL int eanx(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN system barcodes */
INTERNAL int c39(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 3 from 9 (or Code 39) */
INTERNAL int pharmazentral(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmazentral Nummer (PZN) */
INTERNAL int ec39(struct zint_symbol *symbol, unsigned char source[], int length); /* Extended Code 3 from 9 (or Code 39+) */
INTERNAL int codabar(struct zint_symbol *symbol, unsigned char source[], int length); /* Codabar - a simple substitution cipher */
INTERNAL int matrix_two_of_five(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Standard (& Matrix) */
INTERNAL int industrial_two_of_five(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Industrial */
INTERNAL int iata_two_of_five(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 IATA */
INTERNAL int interleaved_two_of_five(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Interleaved */
INTERNAL int logic_two_of_five(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 2 of 5 Data Logic */
INTERNAL int itf14(struct zint_symbol *symbol, unsigned char source[], int length); /* ITF-14 */
INTERNAL int dpleit(struct zint_symbol *symbol, unsigned char source[], int length); /* Deutsche Post Leitcode */
INTERNAL int dpident(struct zint_symbol *symbol, unsigned char source[], int length); /* Deutsche Post Identcode */
INTERNAL int c93(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 93 - a re-working of Code 39+, generates 2 check digits */
INTERNAL int code_128(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 128 and NVE-18 */
INTERNAL int ean_128(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN-128 (GS1-128) */
INTERNAL int code_11(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 11 */
INTERNAL int msi_handle(struct zint_symbol *symbol, unsigned char source[], int length); /* MSI Plessey */
INTERNAL int telepen(struct zint_symbol *symbol, unsigned char source[], int length); /* Telepen ASCII */
INTERNAL int telepen_num(struct zint_symbol *symbol, unsigned char source[], int length); /* Telepen Numeric */
INTERNAL int plessey(struct zint_symbol *symbol, unsigned char source[], int length); /* Plessey Code */
INTERNAL int pharma_one(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmacode One Track */
INTERNAL int flattermarken(struct zint_symbol *symbol, unsigned char source[], int length); /* Flattermarken */
INTERNAL int fim(struct zint_symbol *symbol, unsigned char source[], int length); /* Facing Identification Mark */
INTERNAL int pharma_two(struct zint_symbol *symbol, unsigned char source[], int length); /* Pharmacode Two Track */
INTERNAL int post_plot(struct zint_symbol *symbol, unsigned char source[], int length); /* Postnet */
INTERNAL int planet_plot(struct zint_symbol *symbol, unsigned char source[], int length); /* PLANET */
INTERNAL int imail(struct zint_symbol *symbol, unsigned char source[], int length); /* Intelligent Mail (aka USPS OneCode) */
INTERNAL int royal_plot(struct zint_symbol *symbol, unsigned char source[], int length); /* RM4SCC */
INTERNAL int australia_post(struct zint_symbol *symbol, unsigned char source[], int length); /* Australia Post 4-state */
INTERNAL int code16k(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 16k */
INTERNAL int pdf417enc(struct zint_symbol *symbol, unsigned char source[], int length); /* PDF417 */
INTERNAL int micro_pdf417(struct zint_symbol *symbol, unsigned char chaine[], int length); /* Micro PDF417 */
INTERNAL int maxicode(struct zint_symbol *symbol, unsigned char source[], int length); /* Maxicode */
INTERNAL int rss14(struct zint_symbol *symbol, unsigned char source[], int length); /* RSS-14 */
INTERNAL int rsslimited(struct zint_symbol *symbol, unsigned char source[], int length); /* RSS Limited */
INTERNAL int rssexpanded(struct zint_symbol *symbol, unsigned char source[], int length); /* RSS Expanded */
INTERNAL int composite(struct zint_symbol *symbol, unsigned char source[], int length); /* Composite Symbology */
INTERNAL int kix_code(struct zint_symbol *symbol, unsigned char source[], int length); /* TNT KIX Code */
INTERNAL int aztec(struct zint_symbol *symbol, unsigned char source[], int length); /* Aztec Code */
INTERNAL int code32(struct zint_symbol *symbol, unsigned char source[], int length); /* Italian Pharmacode */
INTERNAL int daft_code(struct zint_symbol *symbol, unsigned char source[], int length); /* DAFT Code */
INTERNAL int ean_14(struct zint_symbol *symbol, unsigned char source[], int length); /* EAN-14 */
INTERNAL int nve_18(struct zint_symbol *symbol, unsigned char source[], int length); /* NVE-18 */
INTERNAL int microqr(struct zint_symbol *symbol, unsigned char source[], int length); /* Micro QR Code */
INTERNAL int aztec_runes(struct zint_symbol *symbol, unsigned char source[], int length); /* Aztec Runes */
INTERNAL int korea_post(struct zint_symbol *symbol, unsigned char source[], int length); /* Korea Post */
INTERNAL int japan_post(struct zint_symbol *symbol, unsigned char source[], int length); /* Japanese Post */
INTERNAL int code_49(struct zint_symbol *symbol, unsigned char source[], int length); /* Code 49 */
INTERNAL int channel_code(struct zint_symbol *symbol, unsigned char source[], int length); /* Channel Code */
INTERNAL int code_one(struct zint_symbol *symbol, unsigned char source[], int length); /* Code One */
INTERNAL int grid_matrix(struct zint_symbol *symbol, unsigned char source[], int length); /* Grid Matrix */
INTERNAL int han_xin(struct zint_symbol *symbol, unsigned char source[], int length); /* Han Xin */
INTERNAL int dotcode(struct zint_symbol *symbol, unsigned char source[], int length); /* DotCode */
INTERNAL int codablock(struct zint_symbol *symbol, unsigned char source[], int length); /* Codablock */
INTERNAL int upnqr(struct zint_symbol *symbol, unsigned char source[], int length); /* UPNQR */
INTERNAL int qr_code(struct zint_symbol *symbol, unsigned char source[], int length); /* QR Code */
INTERNAL int dmatrix(struct zint_symbol *symbol, unsigned char source[], int length); /* Data Matrix (IEC16022) */
INTERNAL int vin(struct zint_symbol *symbol, unsigned char source[], int length); /* VIN Code (Vehicle Identification Number) */
INTERNAL int mailmark(struct zint_symbol *symbol, unsigned char source[], int length); /* Royal Mail 4-state Mailmark */
INTERNAL int ultracode(struct zint_symbol *symbol, unsigned char source[], int length); /* Ultracode */
INTERNAL int rmqr(struct zint_symbol *symbol, unsigned char source[], int length); /* rMQR */
INTERNAL int dpd_parcel(struct zint_symbol *symbol, unsigned char source[], int length); /* DPD Code */

INTERNAL int plot_raster(struct zint_symbol *symbol, int rotate_angle, int file_type); /* Plot to PNG/BMP/PCX */
INTERNAL int plot_vector(struct zint_symbol *symbol, int rotate_angle, int file_type); /* Plot to EPS/EMF/SVG */

STATIC_UNLESS_ZINT_TEST int error_tag(char error_string[100], int error_number) {

    if (error_number != 0) {
        const char *fmt = error_number >= ZINT_ERROR ? "Error %.93s" : "Warning %.91s"; /* Truncate if too long */
        char error_buffer[100];
        strcpy(error_buffer, error_string);

        sprintf(error_string, fmt, error_buffer);
    }

    return error_number;
}

/* Output a hexadecimal representation of the rendered symbol */
static int dump_plot(struct zint_symbol *symbol) {
    FILE *f;
    int i, r;
    char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
        '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    int space = 0;

    if (symbol->output_options & BARCODE_STDOUT) {
        f = stdout;
    } else {
        f = fopen(symbol->outfile, "w");
        if (!f) {
            strcpy(symbol->errtxt, "201: Could not open output file");
            return ZINT_ERROR_FILE_ACCESS;
        }
    }

    for (r = 0; r < symbol->rows; r++) {
        int byt = 0;
        for (i = 0; i < symbol->width; i++) {
            byt = byt << 1;
            if (symbol->symbology == BARCODE_ULTRA) {
                if (module_colour_is_set(symbol, r, i)) {
                    byt += 1;
                }
            } else {
                if (module_is_set(symbol, r, i)) {
                    byt += 1;
                }
            }
            if (((i + 1) % 4) == 0) {
                fputc(hex[byt], f);
                space++;
                byt = 0;
            }
            if (space == 2 && i + 1 < symbol->width) {
                fputc(' ', f);
                space = 0;
            }
        }

        if ((symbol->width % 4) != 0) {
            byt = byt << (4 - (symbol->width % 4));
            fputc(hex[byt], f);
        }
        fputs("\n", f);
        space = 0;
    }

    if (symbol->output_options & BARCODE_STDOUT) {
        fflush(f);
    } else {
        fclose(f);
    }

    return 0;
}

/* Process health industry bar code data */
static int hibc(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i;
    int    counter, error_number;
    char to_process[113], check_digit;

    /* without "+" and check: max 110 characters in HIBC 2.6 */
    if (length > 110) {
        strcpy(symbol->errtxt, "202: Data too long for HIBC LIC");
        return ZINT_ERROR_TOO_LONG;
    }
    to_upper(source);
    error_number = is_sane(TECHNETIUM, source, length);
    if (error_number == ZINT_ERROR_INVALID_DATA) {
        strcpy(symbol->errtxt, "203: Invalid characters in data");
        return error_number;
    }

    counter = 41;
    for (i = 0; i < length; i++) {
        counter += posn(TECHNETIUM, source[i]);
    }
    counter = counter % 43;

    if (counter < 10) {
        check_digit = itoc(counter);
    } else {
        if (counter < 36) {
            check_digit = (counter - 10) + 'A';
        } else {
            switch (counter) {
                case 36: check_digit = '-';
                    break;
                case 37: check_digit = '.';
                    break;
                case 38: check_digit = ' ';
                    break;
                case 39: check_digit = '$';
                    break;
                case 40: check_digit = '/';
                    break;
                case 41: check_digit = '+';
                    break;
                case 42: check_digit = '%';
                    break;
                default: check_digit = ' ';
                    break; /* Keep compiler happy */
            }
        }
    }

    to_process[0] = '+';
    memcpy(to_process + 1, source, length);
    to_process[length + 1] = check_digit;
    length += 2;
    to_process[length] = '\0';

    switch (symbol->symbology) {
        case BARCODE_HIBC_128:
            error_number = code_128(symbol, (unsigned char *) to_process, length);
            ustrcpy(symbol->text, "*");
            ustrcat(symbol->text, to_process);
            ustrcat(symbol->text, "*");
            break;
        case BARCODE_HIBC_39:
            symbol->option_2 = 0;
            error_number = c39(symbol, (unsigned char *) to_process, length);
            ustrcpy(symbol->text, "*");
            ustrcat(symbol->text, to_process);
            ustrcat(symbol->text, "*");
            break;
        case BARCODE_HIBC_DM:
            error_number = dmatrix(symbol, (unsigned char *) to_process, length);
            break;
        case BARCODE_HIBC_QR:
            error_number = qr_code(symbol, (unsigned char *) to_process, length);
            break;
        case BARCODE_HIBC_PDF:
            error_number = pdf417enc(symbol, (unsigned char *) to_process, length);
            break;
        case BARCODE_HIBC_MICPDF:
            error_number = micro_pdf417(symbol, (unsigned char *) to_process, length);
            break;
        case BARCODE_HIBC_AZTEC:
            error_number = aztec(symbol, (unsigned char *) to_process, length);
            break;
        case BARCODE_HIBC_BLOCKF:
            error_number = codablock(symbol, (unsigned char *) to_process, length);
            break;
    }

    return error_number;
}

static void check_row_heights(struct zint_symbol *symbol) {
    /* Check that rows with undefined heights are never less than 5x  */
    int large_bar_count = 0;
    int i;
    int preset_height = 0;
    int large_bar_height = 0;

    for (i = 0; i < symbol->rows; i++) {
        preset_height += symbol->row_height[i];
        if (symbol->row_height[i] == 0) {
            large_bar_count++;
        }
    }

    if (large_bar_count == 0) {
        symbol->height = preset_height;
    } else {
        large_bar_height = (symbol->height - preset_height) / large_bar_count;
    }

    if (large_bar_height < 5) {
        for (i = 0; i < symbol->rows; i++) {
            if (symbol->row_height[i] == 0) {
                symbol->row_height[i] = 5;
                preset_height += 5;
            }
        }
        symbol->height = preset_height;
    }
}

static int check_force_gs1(const int symbology) {
    /* Returns 1 if symbology MUST have GS1 data */

    int result = is_composite(symbology);

    switch (symbology) {
        case BARCODE_GS1_128:
        case BARCODE_EAN14:
        case BARCODE_NVE18:
        case BARCODE_DBAR_EXP:
        case BARCODE_DBAR_EXPSTK:
            result = 1;
            break;
    }

    return result;
}

static int gs1_compliant(const int symbology) {
    /* Returns 1 if symbology supports GS1 data */

    int result = check_force_gs1(symbology);

    switch (symbology) {
        case BARCODE_CODE16K:
        case BARCODE_AZTEC:
        case BARCODE_DATAMATRIX:
        case BARCODE_CODEONE:
        case BARCODE_CODE49:
        case BARCODE_QRCODE:
        case BARCODE_DOTCODE:
        case BARCODE_RMQR:
        case BARCODE_ULTRA:
            result = 1;
            break;
    }

    return result;
}

static int is_dotty(const int symbology) {
    /* Returns 1 if symbology is a matrix design renderable as dots */

    int result = 0;

    switch (symbology) {
        /* Note MAXICODE and ULTRA absent */
        case BARCODE_QRCODE:
        case BARCODE_DATAMATRIX:
        case BARCODE_MICROQR:
        case BARCODE_HIBC_DM:
        case BARCODE_AZTEC:
        case BARCODE_HIBC_QR:
        case BARCODE_HIBC_AZTEC:
        case BARCODE_AZRUNE:
        case BARCODE_CODEONE:
        case BARCODE_GRIDMATRIX:
        case BARCODE_HANXIN:
        case BARCODE_DOTCODE:
        case BARCODE_UPNQR:
        case BARCODE_RMQR:
            result = 1;
            break;
    }

    return result;
}

static int is_fixed_ratio(const int symbology) {
    /* Returns 1 if symbology has fixed aspect ratio (matrix design) */

    int result = 0;

    if (is_dotty(symbology)) {
        return 1;
    }

    switch (symbology) {
        case BARCODE_MAXICODE:
        case BARCODE_ULTRA:
            result = 1;
            break;
    }

    return result;
}

static int is_linear(const int symbology) {
    /* Returns 1 if symbology is linear (1 dimensional) */

    int result = 0;
    switch (symbology) {
        case BARCODE_CODE11:
        case BARCODE_C25STANDARD:
        case BARCODE_C25INTER:
        case BARCODE_C25IATA:
        case BARCODE_C25LOGIC:
        case BARCODE_C25IND:
        case BARCODE_CODE39:
        case BARCODE_EXCODE39:
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
        case BARCODE_GS1_128:
        case BARCODE_CODABAR:
        case BARCODE_CODE128:
        case BARCODE_DPLEIT:
        case BARCODE_DPIDENT:
        case BARCODE_CODE93:
        case BARCODE_FLAT:
        case BARCODE_DBAR_OMN:
        case BARCODE_DBAR_LTD:
        case BARCODE_DBAR_EXP:
        case BARCODE_TELEPEN:
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
        case BARCODE_MSI_PLESSEY:
        case BARCODE_FIM:
        case BARCODE_LOGMARS:
        case BARCODE_PHARMA:
        case BARCODE_PZN:
        case BARCODE_CODE128B:
        case BARCODE_ISBNX:
        case BARCODE_EAN14:
        case BARCODE_NVE18:
        case BARCODE_KOREAPOST:
        case BARCODE_USPS_IMAIL:
        case BARCODE_PLESSEY:
        case BARCODE_TELEPEN_NUM:
        case BARCODE_ITF14:
        case BARCODE_HIBC_128:
        case BARCODE_HIBC_39:
        case BARCODE_CODE32:
        case BARCODE_EANX_CC:
        case BARCODE_GS1_128_CC:
        case BARCODE_DBAR_OMN_CC:
        case BARCODE_DBAR_LTD_CC:
        case BARCODE_DBAR_EXP_CC:
        case BARCODE_UPCA_CC:
        case BARCODE_UPCE_CC:
        case BARCODE_CHANNEL:
        case BARCODE_VIN:
        case BARCODE_DPD:
            result = 1;
            break;
    }

    return result;
}

static int supports_eci(const int symbology) {
    /* Returns 1 if symbology can encode the ECI character */

    int result = 0;

    switch (symbology) {
        case BARCODE_AZTEC:
        case BARCODE_DATAMATRIX:
        case BARCODE_MAXICODE:
        case BARCODE_MICROPDF417:
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
        case BARCODE_QRCODE:
        case BARCODE_DOTCODE:
        case BARCODE_CODEONE:
        case BARCODE_GRIDMATRIX:
        case BARCODE_HANXIN:
        case BARCODE_ULTRA:
            result = 1;
            break;
    }

    return result;
}

static int has_hrt(const int symbology) {
    /* Returns 1 if symbology supports HRT */

    if (is_fixed_ratio(symbology)) {
        return 0;
    }

    switch (symbology) { /* These don't support HRT */
        case BARCODE_CODE16K:
        case BARCODE_CODE49:
        case BARCODE_FLAT:
        case BARCODE_POSTNET:
        case BARCODE_FIM:
        case BARCODE_PHARMA:
        case BARCODE_PHARMA_TWO:
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
        case BARCODE_AUSPOST:
        case BARCODE_AUSREPLY:
        case BARCODE_AUSROUTE:
        case BARCODE_AUSREDIRECT:
        case BARCODE_RM4SCC:
        case BARCODE_CODABLOCKF:
        case BARCODE_JAPANPOST:
        case BARCODE_DBAR_STK:
        case BARCODE_DBAR_OMNSTK:
        case BARCODE_DBAR_EXPSTK:
        case BARCODE_PLANET:
        case BARCODE_MICROPDF417:
        case BARCODE_USPS_IMAIL:
        case BARCODE_KIX:
        case BARCODE_DAFT:
        case BARCODE_HIBC_PDF:
        case BARCODE_HIBC_MICPDF:
        case BARCODE_HIBC_BLOCKF:
        case BARCODE_MAILMARK:
        case BARCODE_DBAR_STK_CC:
        case BARCODE_DBAR_OMNSTK_CC:
        case BARCODE_DBAR_EXPSTK_CC:
            return 0;
            break;
    }

    return 1;
}

unsigned int ZBarcode_Cap(int symbol_id, unsigned int cap_flag) {
    unsigned int result = 0;

    if (!ZBarcode_ValidID(symbol_id)) {
        return 0;
    }

    if ((cap_flag & ZINT_CAP_HRT) && has_hrt(symbol_id)) {
        result |= ZINT_CAP_HRT;
    }
    if ((cap_flag & ZINT_CAP_STACKABLE) && is_stackable(symbol_id)) {
        result |= ZINT_CAP_STACKABLE;
    }
    if ((cap_flag & ZINT_CAP_EXTENDABLE) && is_extendable(symbol_id)) {
        result |= ZINT_CAP_EXTENDABLE;
    }
    if ((cap_flag & ZINT_CAP_COMPOSITE) && is_composite(symbol_id)) {
        result |= ZINT_CAP_COMPOSITE;
    }
    if ((cap_flag & ZINT_CAP_ECI) && supports_eci(symbol_id)) {
        result |= ZINT_CAP_ECI;
    }
    if ((cap_flag & ZINT_CAP_GS1) && gs1_compliant(symbol_id)) {
        result |= ZINT_CAP_GS1;
    }
    if ((cap_flag & ZINT_CAP_DOTTY) && is_dotty(symbol_id)) {
        result |= ZINT_CAP_DOTTY;
    }
    if ((cap_flag & ZINT_CAP_FIXED_RATIO) && is_fixed_ratio(symbol_id)) {
        result |= ZINT_CAP_FIXED_RATIO;
    }
    if (cap_flag & ZINT_CAP_READER_INIT) {
        /* Note does not include HIBC versions */
        switch (symbol_id) {
            case BARCODE_CODE128: /* Note does not include GS1_128 or NVE18 */
            case BARCODE_CODE128B:
            case BARCODE_CODE16K:
            case BARCODE_CODABLOCKF:
            case BARCODE_PDF417:
            case BARCODE_PDF417COMP:
            case BARCODE_DATAMATRIX:
            case BARCODE_MICROPDF417:
            case BARCODE_AZTEC:
            case BARCODE_DOTCODE:
            case BARCODE_GRIDMATRIX:
            case BARCODE_ULTRA:
                result |= ZINT_CAP_READER_INIT;
                break;
        }
    }
    if (cap_flag & ZINT_CAP_FULL_MULTIBYTE) {
        switch (symbol_id) {
            case BARCODE_QRCODE:
            case BARCODE_MICROQR:
            //case BARCODE_HIBC_QR: Note character set restricted to ASCII subset
            //case BARCODE_UPNQR: Note does not use Kanji mode
            case BARCODE_RMQR:
            case BARCODE_HANXIN:
            case BARCODE_GRIDMATRIX:
                result |= ZINT_CAP_FULL_MULTIBYTE;
                break;
        }
    }
    if (cap_flag & ZINT_CAP_MASK) {
        switch (symbol_id) {
            case BARCODE_QRCODE:
            case BARCODE_MICROQR:
            case BARCODE_HANXIN:
            case BARCODE_DOTCODE:
                result |= ZINT_CAP_MASK;
                break;
        }
    }

    return result;
}

int ZBarcode_ValidID(int symbol_id) {
    /* Checks whether a symbology is supported */
    static const unsigned char ids[146] = {
          0,   1,   2,   3,   4,   0,   6,   7,   8,   9,
          0,   0,   0,  13,  14,   0,  16,   0,  18,   0,
         20,  21,  22,  23,  24,  25,   0,   0,  28,  29,
         30,  31,  32,   0,  34,  35,   0,  37,  38,   0,
         40,   0,   0,   0,   0,   0,   0,  47,   0,  49,
         50,  51,  52,  53,   0,  55,  56,  57,  58,   0,
         60,   0,   0,  63,   0,   0,  66,  67,  68,  69,
         70,  71,  72,  73,  74,  75,  76,  77,   0,  79,
         80,  81,  82,   0,  84,  85,  86,  87,   0,  89,
         90,   0,  92,  93,   0,   0,  96,  97,  98,  99,
          0,   0, 102,   0, 104,   0, 106,   0, 108,   0,
        110,   0, 112,   0,   0, 115, 116,   0,   0,   0,
          0, 121,   0,   0,   0,   0,   0,   0, 128, 129,
        130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
        140, 141, 142, 143, 144, 145,
    };

    if (symbol_id <= 0 || symbol_id > 145) {
        return 0;
    }

    return ids[symbol_id] != 0;
}

static int reduced_charset(struct zint_symbol *symbol, unsigned char *source, int in_length);

static int extended_or_reduced_charset(struct zint_symbol *symbol, unsigned char *source, const int length) {
    int error_number = 0;

    switch (symbol->symbology) {
        /* These are the "elite" standards which have support for specific character sets */
        case BARCODE_QRCODE: error_number = qr_code(symbol, source, length);
            break;
        case BARCODE_MICROQR: error_number = microqr(symbol, source, length);
            break;
        case BARCODE_GRIDMATRIX: error_number = grid_matrix(symbol, source, length);
            break;
        case BARCODE_HANXIN: error_number = han_xin(symbol, source, length);
            break;
        case BARCODE_UPNQR: error_number = upnqr(symbol, source, length);
            break;
        case BARCODE_RMQR: error_number = rmqr(symbol, source, length);
            break;
        default: error_number = reduced_charset(symbol, source, length);
            break;
    }

    return error_number;
}

static int reduced_charset(struct zint_symbol *symbol, unsigned char *source, int in_length) {
    /* These are the "norm" standards which only support Latin-1 at most, though a few support ECI */
    int error_number = 0;
    unsigned char *preprocessed = source;

    int eci_length = get_eci_length(symbol->eci, source, in_length);
#ifndef _MSC_VER
    unsigned char preprocessed_buf[eci_length + 1];
#else
    unsigned char *preprocessed_buf = (unsigned char *) _alloca(eci_length + 1);
#endif

    if ((symbol->input_mode & 0x07) == UNICODE_MODE && is_eci_convertible(symbol->eci)) {
        /* Prior check ensures ECI only set for those that support it */
        preprocessed = preprocessed_buf;
        error_number = utf8_to_eci(symbol->eci, source, preprocessed, &in_length);
        if (error_number != 0) {
            strcpy(symbol->errtxt, "204: Invalid characters in input data");
            return error_number;
        }
    }

    if ((symbol->height == 0) && is_linear(symbol->symbology)) {
        symbol->height = 50;
    }

    switch (symbol->symbology) {
        case BARCODE_C25STANDARD: error_number = matrix_two_of_five(symbol, preprocessed, in_length);
            break;
        case BARCODE_C25IND: error_number = industrial_two_of_five(symbol, preprocessed, in_length);
            break;
        case BARCODE_C25INTER: error_number = interleaved_two_of_five(symbol, preprocessed, in_length);
            break;
        case BARCODE_C25IATA: error_number = iata_two_of_five(symbol, preprocessed, in_length);
            break;
        case BARCODE_C25LOGIC: error_number = logic_two_of_five(symbol, preprocessed, in_length);
            break;
        case BARCODE_DPLEIT: error_number = dpleit(symbol, preprocessed, in_length);
            break;
        case BARCODE_DPIDENT: error_number = dpident(symbol, preprocessed, in_length);
            break;
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
            error_number = eanx(symbol, preprocessed, in_length);
            break;
        case BARCODE_GS1_128: error_number = ean_128(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE39: error_number = c39(symbol, preprocessed, in_length);
            break;
        case BARCODE_PZN: error_number = pharmazentral(symbol, preprocessed, in_length);
            break;
        case BARCODE_EXCODE39: error_number = ec39(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODABAR: error_number = codabar(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE93: error_number = c93(symbol, preprocessed, in_length);
            break;
        case BARCODE_LOGMARS: error_number = c39(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE128:
        case BARCODE_CODE128B:
            error_number = code_128(symbol, preprocessed, in_length);
            break;
        case BARCODE_NVE18: error_number = nve_18(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE11: error_number = code_11(symbol, preprocessed, in_length);
            break;
        case BARCODE_MSI_PLESSEY: error_number = msi_handle(symbol, preprocessed, in_length);
            break;
        case BARCODE_TELEPEN: error_number = telepen(symbol, preprocessed, in_length);
            break;
        case BARCODE_TELEPEN_NUM: error_number = telepen_num(symbol, preprocessed, in_length);
            break;
        case BARCODE_PHARMA: error_number = pharma_one(symbol, preprocessed, in_length);
            break;
        case BARCODE_PLESSEY: error_number = plessey(symbol, preprocessed, in_length);
            break;
        case BARCODE_ITF14: error_number = itf14(symbol, preprocessed, in_length);
            break;
        case BARCODE_FLAT: error_number = flattermarken(symbol, preprocessed, in_length);
            break;
        case BARCODE_FIM: error_number = fim(symbol, preprocessed, in_length);
            break;
        case BARCODE_POSTNET: error_number = post_plot(symbol, preprocessed, in_length);
            break;
        case BARCODE_PLANET: error_number = planet_plot(symbol, preprocessed, in_length);
            break;
        case BARCODE_RM4SCC: error_number = royal_plot(symbol, preprocessed, in_length);
            break;
        case BARCODE_AUSPOST:
        case BARCODE_AUSREPLY:
        case BARCODE_AUSROUTE:
        case BARCODE_AUSREDIRECT:
            error_number = australia_post(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE16K: error_number = code16k(symbol, preprocessed, in_length);
            break;
        case BARCODE_PHARMA_TWO: error_number = pharma_two(symbol, preprocessed, in_length);
            break;
        case BARCODE_USPS_IMAIL: error_number = imail(symbol, preprocessed, in_length);
            break;
        case BARCODE_ISBNX: error_number = eanx(symbol, preprocessed, in_length);
            break;
        case BARCODE_DBAR_OMN:
        case BARCODE_DBAR_STK:
        case BARCODE_DBAR_OMNSTK:
            error_number = rss14(symbol, preprocessed, in_length);
            break;
        case BARCODE_DBAR_LTD: error_number = rsslimited(symbol, preprocessed, in_length);
            break;
        case BARCODE_DBAR_EXP:
        case BARCODE_DBAR_EXPSTK:
            error_number = rssexpanded(symbol, preprocessed, in_length);
            break;
        case BARCODE_EANX_CC:
        case BARCODE_GS1_128_CC:
        case BARCODE_DBAR_OMN_CC:
        case BARCODE_DBAR_LTD_CC:
        case BARCODE_DBAR_EXP_CC:
        case BARCODE_UPCA_CC:
        case BARCODE_UPCE_CC:
        case BARCODE_DBAR_STK_CC:
        case BARCODE_DBAR_OMNSTK_CC:
        case BARCODE_DBAR_EXPSTK_CC:
            error_number = composite(symbol, preprocessed, in_length);
            break;
        case BARCODE_KIX: error_number = kix_code(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE32: error_number = code32(symbol, preprocessed, in_length);
            break;
        case BARCODE_DAFT: error_number = daft_code(symbol, preprocessed, in_length);
            break;
        case BARCODE_EAN14:
            error_number = ean_14(symbol, preprocessed, in_length);
            break;
        case BARCODE_AZRUNE: error_number = aztec_runes(symbol, preprocessed, in_length);
            break;
        case BARCODE_KOREAPOST: error_number = korea_post(symbol, preprocessed, in_length);
            break;
        case BARCODE_HIBC_128:
        case BARCODE_HIBC_39:
        case BARCODE_HIBC_DM:
        case BARCODE_HIBC_QR:
        case BARCODE_HIBC_PDF:
        case BARCODE_HIBC_MICPDF:
        case BARCODE_HIBC_AZTEC:
        case BARCODE_HIBC_BLOCKF:
            error_number = hibc(symbol, preprocessed, in_length);
            break;
        case BARCODE_JAPANPOST: error_number = japan_post(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODE49: error_number = code_49(symbol, preprocessed, in_length);
            break;
        case BARCODE_CHANNEL: error_number = channel_code(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODEONE: error_number = code_one(symbol, preprocessed, in_length);
            break;
        case BARCODE_DATAMATRIX: error_number = dmatrix(symbol, preprocessed, in_length);
            break;
        case BARCODE_PDF417:
        case BARCODE_PDF417COMP:
            error_number = pdf417enc(symbol, preprocessed, in_length);
            break;
        case BARCODE_MICROPDF417: error_number = micro_pdf417(symbol, preprocessed, in_length);
            break;
        case BARCODE_MAXICODE: error_number = maxicode(symbol, preprocessed, in_length);
            break;
        case BARCODE_AZTEC: error_number = aztec(symbol, preprocessed, in_length);
            break;
        case BARCODE_DOTCODE: error_number = dotcode(symbol, preprocessed, in_length);
            break;
        case BARCODE_CODABLOCKF: error_number = codablock(symbol, preprocessed, in_length);
            break;
        case BARCODE_VIN: error_number = vin(symbol, preprocessed, in_length);
            break;
        case BARCODE_MAILMARK: error_number = mailmark(symbol, preprocessed, in_length);
            break;
        case BARCODE_ULTRA: error_number = ultracode(symbol, preprocessed, in_length);
            break;
        case BARCODE_DPD: error_number = dpd_parcel(symbol, preprocessed, in_length);
            break;
    }

    return error_number;
}

STATIC_UNLESS_ZINT_TEST void strip_bom(unsigned char *source, int *input_length) {
    int i;

    /* Note if BOM is only data then not stripped */
    if (*input_length > 3 && (source[0] == 0xef) && (source[1] == 0xbb) && (source[2] == 0xbf)) {
        /* BOM at start of input data, strip in accordance with RFC 3629 */
        for (i = 3; i <= *input_length; i++) { /* Include terminating NUL */
            source[i - 3] = source[i];
        }
        *input_length -= 3;
    }
}

static int escape_char_process(struct zint_symbol *symbol, unsigned char *input_string, int *length) {
    int error_number;
    int in_posn, out_posn;
    int hex1, hex2;
    int i, unicode;

#ifndef _MSC_VER
    unsigned char escaped_string[*length + 1];
#else
    unsigned char *escaped_string = (unsigned char *) _alloca(*length + 1);
#endif

    in_posn = 0;
    out_posn = 0;

    do {
        if (input_string[in_posn] == '\\') {
            if (in_posn + 1 >= *length) {
                strcpy(symbol->errtxt, "236: Incomplete escape character in input data");
                return ZINT_ERROR_INVALID_DATA;
            }
            switch (input_string[in_posn + 1]) {
                case '0': escaped_string[out_posn] = 0x00; /* Null */
                    in_posn += 2;
                    break;
                case 'E': escaped_string[out_posn] = 0x04; /* End of Transmission */
                    in_posn += 2;
                    break;
                case 'a': escaped_string[out_posn] = 0x07; /* Bell */
                    in_posn += 2;
                    break;
                case 'b': escaped_string[out_posn] = 0x08; /* Backspace */
                    in_posn += 2;
                    break;
                case 't': escaped_string[out_posn] = 0x09; /* Horizontal tab */
                    in_posn += 2;
                    break;
                case 'n': escaped_string[out_posn] = 0x0a; /* Line feed */
                    in_posn += 2;
                    break;
                case 'v': escaped_string[out_posn] = 0x0b; /* Vertical tab */
                    in_posn += 2;
                    break;
                case 'f': escaped_string[out_posn] = 0x0c; /* Form feed */
                    in_posn += 2;
                    break;
                case 'r': escaped_string[out_posn] = 0x0d; /* Carriage return */
                    in_posn += 2;
                    break;
                case 'e': escaped_string[out_posn] = 0x1b; /* Escape */
                    in_posn += 2;
                    break;
                case 'G': escaped_string[out_posn] = 0x1d; /* Group Separator */
                    in_posn += 2;
                    break;
                case 'R': escaped_string[out_posn] = 0x1e; /* Record Separator */
                    in_posn += 2;
                    break;
                case 'x': if (in_posn + 4 > *length) {
                        strcpy(symbol->errtxt, "232: Incomplete escape character in input data");
                        return ZINT_ERROR_INVALID_DATA;
                    }
                    hex1 = ctoi(input_string[in_posn + 2]);
                    hex2 = ctoi(input_string[in_posn + 3]);
                    if ((hex1 >= 0) && (hex2 >= 0)) {
                        escaped_string[out_posn] = (hex1 << 4) + hex2;
                        in_posn += 4;
                    } else {
                        strcpy(symbol->errtxt, "233: Corrupt escape character in input data");
                        return ZINT_ERROR_INVALID_DATA;
                    }
                    break;
                case '\\': escaped_string[out_posn] = '\\';
                    in_posn += 2;
                    break;
                case 'u':
                    if (in_posn + 6 > *length) {
                        strcpy(symbol->errtxt, "209: Incomplete Unicode escape character in input data");
                        return ZINT_ERROR_INVALID_DATA;
                    }
                    unicode = 0;
                    for (i = 0; i < 4; i++) {
                        if (ctoi(input_string[in_posn + i + 2]) == -1) {
                            strcpy(symbol->errtxt, "211: Corrupt Unicode escape character in input data");
                            return ZINT_ERROR_INVALID_DATA;
                        }
                        unicode = unicode << 4;
                        unicode += ctoi(input_string[in_posn + i + 2]);
                    }
                    /* Exclude reversed BOM and surrogates */
                    if (unicode == 0xfffe || (unicode >= 0xd800 && unicode < 0xe000)) {
                        strcpy(symbol->errtxt, "246: Invalid Unicode BMP escape character in input data");
                        return ZINT_ERROR_INVALID_DATA;
                    }
                    if (unicode >= 0x800) {
                        escaped_string[out_posn] = 0xe0 + ((unicode & 0xf000) >> 12);
                        out_posn++;
                        escaped_string[out_posn] = 0x80 + ((unicode & 0x0fc0) >> 6);
                        out_posn++;
                        escaped_string[out_posn] = 0x80 + (unicode & 0x003f);
                    } else if (unicode >= 0x80) {
                        escaped_string[out_posn] = 0xc0 + ((unicode & 0x07c0) >> 6);
                        out_posn++;
                        escaped_string[out_posn] = 0x80 + (unicode & 0x003f);
                    } else {
                        escaped_string[out_posn] = unicode & 0x7f;
                    }
                    in_posn += 6;
                    break;
                default: strcpy(symbol->errtxt, "234: Unrecognised escape character in input data");
                    return ZINT_ERROR_INVALID_DATA;
                    break;
            }
        } else {
            escaped_string[out_posn] = input_string[in_posn];
            in_posn++;
        }
        out_posn++;
    } while (in_posn < *length);

    memcpy(input_string, escaped_string, out_posn);
    input_string[out_posn] = '\0';
    *length = out_posn;

    error_number = 0;

    return error_number;
}

int ZBarcode_Encode(struct zint_symbol *symbol, const unsigned char *source, int in_length) {
    int error_number, warn_number;
#ifdef _MSC_VER
    unsigned char *local_source;
#endif

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("ZBarcode_Encode: symbology: %d, input_mode: 0x%X, ECI: %d, option_1: %d, option_2: %d,"
                " option_3: %d, scale: %g\n    output_options: 0x%X, fg: %s, bg: %s,"
                " in_length: %d, First 10 source: \"%.10s\", First 10 primary: \"%.10s\"\n",
                symbol->symbology, symbol->input_mode, symbol->eci, symbol->option_1, symbol->option_2,
                symbol->option_3, symbol->scale, symbol->output_options, symbol->fgcolour, symbol->bgcolour,
                in_length, source, symbol->primary);
    }

    warn_number = 0;

    if (source == NULL) {
        strcpy(symbol->errtxt, "200: Input data NULL");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
    }
    if (in_length <= 0) {
        in_length = (int) ustrlen(source);
    }
    if (in_length <= 0) {
        strcpy(symbol->errtxt, "205: No input data");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
    }
    if (in_length > ZINT_MAX_DATA_LEN) {
        strcpy(symbol->errtxt, "243: Input data too long");
        return error_tag(symbol->errtxt, ZINT_ERROR_TOO_LONG);
    }

    if (*symbol->outfile == '\0') {
#ifdef NO_PNG
        strcpy(symbol->outfile, "out.gif");
#else
        strcpy(symbol->outfile, "out.png");
#endif
    }

    /* First check the symbology field */
    if (!ZBarcode_ValidID(symbol->symbology)) {
        if (symbol->symbology < 1) {
            strcpy(symbol->errtxt, "206: Symbology out of range");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        /* symbol->symbologys 1 to 86 are defined by tbarcode */
        } else if (symbol->symbology == 5) {
            symbol->symbology = BARCODE_C25STANDARD;
        } else if ((symbol->symbology >= 10) && (symbol->symbology <= 12)) {
            symbol->symbology = BARCODE_EANX;
        } else if (symbol->symbology == 15) {
            symbol->symbology = BARCODE_EANX;
        } else if (symbol->symbology == 17) {
            symbol->symbology = BARCODE_UPCA;
        } else if (symbol->symbology == 19) {
            strcpy(symbol->errtxt, "207: Codabar 18 not supported");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODABAR;
            warn_number = ZINT_WARN_INVALID_OPTION;
        } else if (symbol->symbology == 26) {
            symbol->symbology = BARCODE_UPCA;
        } else if (symbol->symbology == 27) {
            strcpy(symbol->errtxt, "208: UPCD1 not supported");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        } else if (symbol->symbology == 33) {
            symbol->symbology = BARCODE_GS1_128;
        } else if (symbol->symbology == 36) {
            symbol->symbology = BARCODE_UPCA;
        } else if ((symbol->symbology >= 41) && (symbol->symbology <= 45)) {
            symbol->symbology = BARCODE_POSTNET;
        } else if (symbol->symbology == 46) {
            symbol->symbology = BARCODE_PLESSEY;
        } else if (symbol->symbology == 48) {
            symbol->symbology = BARCODE_NVE18;
        } else if (symbol->symbology == 54) {
            strcpy(symbol->errtxt, "210: General Parcel Code not supported");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        } else if ((symbol->symbology == 59) || (symbol->symbology == 61)) {
            symbol->symbology = BARCODE_CODE128;
        } else if (symbol->symbology == 62) {
            symbol->symbology = BARCODE_CODE93;
        } else if ((symbol->symbology == 64) || (symbol->symbology == 65)) {
            symbol->symbology = BARCODE_AUSPOST;
        } else if (symbol->symbology == 78) {
            symbol->symbology = BARCODE_DBAR_OMN;
        } else if (symbol->symbology == 83) {
            symbol->symbology = BARCODE_PLANET;
        } else if (symbol->symbology == 88) {
            symbol->symbology = BARCODE_GS1_128;
        } else if (symbol->symbology == 91) {
            strcpy(symbol->errtxt, "212: Symbology out of range");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        } else if ((symbol->symbology >= 94) && (symbol->symbology <= 95)) {
            strcpy(symbol->errtxt, "213: Symbology out of range");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        } else if (symbol->symbology == 100) {
            symbol->symbology = BARCODE_HIBC_128;
        } else if (symbol->symbology == 101) {
            symbol->symbology = BARCODE_HIBC_39;
        } else if (symbol->symbology == 103) {
            symbol->symbology = BARCODE_HIBC_DM;
        } else if (symbol->symbology == 105) {
            symbol->symbology = BARCODE_HIBC_QR;
        } else if (symbol->symbology == 107) {
            symbol->symbology = BARCODE_HIBC_PDF;
        } else if (symbol->symbology == 109) {
            symbol->symbology = BARCODE_HIBC_MICPDF;
        } else if (symbol->symbology == 111) {
            symbol->symbology = BARCODE_HIBC_BLOCKF;
        } else if ((symbol->symbology == 113) || (symbol->symbology == 114)) {
            strcpy(symbol->errtxt, "214: Symbology out of range");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        } else if (symbol->symbology == 115) {
            symbol->symbology = BARCODE_DOTCODE;
        } else if ((symbol->symbology >= 117) && (symbol->symbology <= 127)) {
            if (symbol->symbology != 121) {
                strcpy(symbol->errtxt, "215: Symbology out of range");
                if (symbol->warn_level == WARN_FAIL_ALL) {
                    return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
                }
                symbol->symbology = BARCODE_CODE128;
                warn_number = ZINT_WARN_INVALID_OPTION;
            }
        /* Everything from 128 up is Zint-specific */
        } else if (symbol->symbology > 145) {
            strcpy(symbol->errtxt, "216: Symbology out of range");
            if (symbol->warn_level == WARN_FAIL_ALL) {
                return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            }
            symbol->symbology = BARCODE_CODE128;
            warn_number = ZINT_WARN_INVALID_OPTION;
        }
    }

    if (symbol->eci != 0) {
        if (!(supports_eci(symbol->symbology))) {
            strcpy(symbol->errtxt, "217: Symbology does not support ECI switching");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
        if ((symbol->eci < 0) || (symbol->eci == 1) || (symbol->eci == 2) || (symbol->eci > 999999)) {
            strcpy(symbol->errtxt, "218: Invalid ECI mode");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    }

    if ((symbol->dot_size < 0.01f) || (symbol->dot_size > 20.0f)) {
        strcpy(symbol->errtxt, "221: Invalid dot size");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
    }

    if ((symbol->input_mode & 0x07) == UNICODE_MODE && !is_valid_utf8(source, in_length)) {
        strcpy(symbol->errtxt, "245: Invalid UTF-8");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
    }

    if ((symbol->input_mode & 0x07) > 2) {
        symbol->input_mode = DATA_MODE; /* Reset completely */
    }

#ifndef _MSC_VER
    unsigned char local_source[in_length + 1];
#else
    local_source = (unsigned char *) _alloca(in_length + 1);
#endif

    memcpy(local_source, source, in_length+1);
    local_source[in_length] = '\0';

    /* Start acting on input mode */
    if (symbol->input_mode & ESCAPE_MODE) {
        error_number = escape_char_process(symbol, local_source, &in_length);
        if (error_number != 0) {
            return error_tag(symbol->errtxt, error_number);
        }
    }

    if ((symbol->input_mode & 0x07) == UNICODE_MODE) {
        strip_bom(local_source, &in_length);
    }

    if (((symbol->input_mode & 0x07) == GS1_MODE) || (check_force_gs1(symbol->symbology))) {
        if (gs1_compliant(symbol->symbology) == 1) {
            // Reduce input for composite and non-forced symbologies, others (EAN128 and RSS_EXP based) will
            // handle it themselves
            if (is_composite(symbol->symbology) || !check_force_gs1(symbol->symbology)) {
#ifndef _MSC_VER
                unsigned char reduced[in_length + 1];
#else
                unsigned char *reduced = (unsigned char *) _alloca(in_length + 1);
#endif
                error_number = gs1_verify(symbol, local_source, in_length, reduced);
                if (error_number >= ZINT_ERROR) {
                    const char in_2d_comp[] = " in 2D component";
                    if (is_composite(symbol->symbology) && strlen(symbol->errtxt) < 100 - strlen(in_2d_comp)) {
                        strcat(symbol->errtxt, in_2d_comp);
                    }
                    return error_tag(symbol->errtxt, error_number);
                }
                if (error_number && warn_number == 0) {
                    warn_number = error_number;
                }
                ustrcpy(local_source, reduced); // Cannot contain nul char
                in_length = (int) ustrlen(local_source);
            }
        } else {
            strcpy(symbol->errtxt, "220: Selected symbology does not support GS1 mode");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    }

    error_number = extended_or_reduced_charset(symbol, local_source, in_length);

    if ((error_number == ZINT_ERROR_INVALID_DATA) && symbol->eci == 0 && supports_eci(symbol->symbology)
            && (symbol->input_mode & 0x07) == UNICODE_MODE) {
        /* Try another ECI mode */
        symbol->eci = get_best_eci(local_source, in_length);
        if (symbol->eci != 0) {
            error_number = extended_or_reduced_charset(symbol, local_source, in_length);
            if (error_number == 0) {
                error_number = ZINT_WARN_USES_ECI;
                if (!(symbol->debug & ZINT_DEBUG_TEST)) {
                    strcpy(symbol->errtxt, "222: Encoded data includes ECI");
                }
                if (symbol->debug & ZINT_DEBUG_PRINT) printf("Added ECI %d\n", symbol->eci);
            }
        }
    }

    if (error_number == 0) {
        error_number = warn_number;
    }
    (void)error_tag(symbol->errtxt, error_number);

    if (error_number < ZINT_ERROR) {
        check_row_heights(symbol);
    }

    return error_number;
}

int ZBarcode_Print(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    switch (rotate_angle) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            strcpy(symbol->errtxt, "223: Invalid rotation angle");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            break;
    }

    if (symbol->output_options & BARCODE_DOTTY_MODE) {
        if (!(is_dotty(symbol->symbology))) {
            strcpy(symbol->errtxt, "224: Selected symbology cannot be rendered as dots");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    }

    if (strlen(symbol->outfile) > 3) {
        char output[4];
        output[0] = symbol->outfile[strlen(symbol->outfile) - 3];
        output[1] = symbol->outfile[strlen(symbol->outfile) - 2];
        output[2] = symbol->outfile[strlen(symbol->outfile) - 1];
        output[3] = '\0';
        to_upper((unsigned char *) output);

        if (!(strcmp(output, "PNG"))) {
            if (symbol->scale < 1.0f) {
                symbol->text[0] = '\0';
            }
            error_number = plot_raster(symbol, rotate_angle, OUT_PNG_FILE);

        } else if (!(strcmp(output, "BMP"))) {
            if (symbol->scale < 1.0f) {
                symbol->text[0] = '\0';
            }
            error_number = plot_raster(symbol, rotate_angle, OUT_BMP_FILE);

        } else if (!(strcmp(output, "PCX"))) {
            if (symbol->scale < 1.0f) {
                symbol->text[0] = '\0';
            }
            error_number = plot_raster(symbol, rotate_angle, OUT_PCX_FILE);

        } else if (!(strcmp(output, "GIF"))) {
            if (symbol->scale < 1.0f) {
                symbol->text[0] = '\0';
            }
            error_number = plot_raster(symbol, rotate_angle, OUT_GIF_FILE);

        } else if (!(strcmp(output, "TIF"))) {
            if (symbol->scale < 1.0f) {
                symbol->text[0] = '\0';
            }
            error_number = plot_raster(symbol, rotate_angle, OUT_TIF_FILE);

        } else if (!(strcmp(output, "TXT"))) {
            error_number = dump_plot(symbol);

        } else if (!(strcmp(output, "EPS"))) {
            error_number = plot_vector(symbol, rotate_angle, OUT_EPS_FILE);

        } else if (!(strcmp(output, "SVG"))) {
            error_number = plot_vector(symbol, rotate_angle, OUT_SVG_FILE);

        } else if (!(strcmp(output, "EMF"))) {
            error_number = plot_vector(symbol, rotate_angle, OUT_EMF_FILE);

        } else {
            strcpy(symbol->errtxt, "225: Unknown output format");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    } else {
        strcpy(symbol->errtxt, "226: Unknown output format");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
    }

    return error_tag(symbol->errtxt, error_number);
}

int ZBarcode_Buffer(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    switch (rotate_angle) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            strcpy(symbol->errtxt, "228: Invalid rotation angle");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            break;
    }

    if (symbol->output_options & BARCODE_DOTTY_MODE) {
        if (!(is_dotty(symbol->symbology))) {
            strcpy(symbol->errtxt, "237: Selected symbology cannot be rendered as dots");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    }

    error_number = plot_raster(symbol, rotate_angle, OUT_BUFFER);
    return error_tag(symbol->errtxt, error_number);
}

int ZBarcode_Buffer_Vector(struct zint_symbol *symbol, int rotate_angle) {
    int error_number;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    switch (rotate_angle) {
        case 0:
        case 90:
        case 180:
        case 270:
            break;
        default:
            strcpy(symbol->errtxt, "219: Invalid rotation angle");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
            break;
    }

    if (symbol->output_options & BARCODE_DOTTY_MODE) {
        if (!(is_dotty(symbol->symbology))) {
            strcpy(symbol->errtxt, "238: Selected symbology cannot be rendered as dots");
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_OPTION);
        }
    }

    error_number = plot_vector(symbol, rotate_angle, OUT_BUFFER);
    return error_tag(symbol->errtxt, error_number);
}

int ZBarcode_Encode_and_Print(struct zint_symbol *symbol, unsigned char *input, int length, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode(symbol, input, length);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Print(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }
    return error_number;
}

int ZBarcode_Encode_and_Buffer(struct zint_symbol *symbol, unsigned char *input, int length, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode(symbol, input, length);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Buffer(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }

    return error_number;
}

int ZBarcode_Encode_and_Buffer_Vector(struct zint_symbol *symbol, unsigned char *input, int length, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode(symbol, input, length);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Buffer_Vector(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }

    return error_number;
}

int ZBarcode_Encode_File(struct zint_symbol *symbol, char *filename) {
    FILE *file;
    int file_opened = 0;
    unsigned char *buffer;
    long fileLen;
    size_t n;
    int nRead = 0;
    int ret;

    if (!symbol) return ZINT_ERROR_INVALID_DATA;

    if (!filename) {
        strcpy(symbol->errtxt, "239: Filename NULL");
        return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
    }

    if (!strcmp(filename, "-")) {
        file = stdin;
        fileLen = ZINT_MAX_DATA_LEN;
    } else {
        file = fopen(filename, "rb");
        if (!file) {
            sprintf(symbol->errtxt, "229: Unable to read input file (%.30s)", strerror(errno));
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
        }
        file_opened = 1;

        /* Get file length */
        fseek(file, 0, SEEK_END);
        fileLen = ftell(file);
        fseek(file, 0, SEEK_SET);

        /* On many Linux distros ftell() returns LONG_MAX not -1 on error */
        if (fileLen <= 0 || fileLen == LONG_MAX) {
            strcpy(symbol->errtxt, "235: Input file empty or unseekable");
            fclose(file);
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
        }
        if (fileLen > ZINT_MAX_DATA_LEN) {
            strcpy(symbol->errtxt, "230: Input file too long");
            fclose(file);
            return error_tag(symbol->errtxt, ZINT_ERROR_TOO_LONG);
        }
    }

    /* Allocate memory */
    buffer = (unsigned char *) malloc(fileLen);
    if (!buffer) {
        strcpy(symbol->errtxt, "231: Internal memory error");
        if (file_opened) {
            fclose(file);
        }
        return error_tag(symbol->errtxt, ZINT_ERROR_MEMORY);
    }

    /* Read file contents into buffer */

    do {
        n = fread(buffer + nRead, 1, fileLen - nRead, file);
        if (ferror(file)) {
            sprintf(symbol->errtxt, "241: Input file read error (%.30s)", strerror(errno));
            if (file_opened) {
                fclose(file);
            }
            free(buffer);
            return error_tag(symbol->errtxt, ZINT_ERROR_INVALID_DATA);
        }
        nRead += n;
    } while (!feof(file) && (0 < n) && (nRead < fileLen));

    if (file_opened) {
        fclose(file);
    }
    ret = ZBarcode_Encode(symbol, buffer, nRead);
    free(buffer);
    return ret;
}

int ZBarcode_Encode_File_and_Print(struct zint_symbol *symbol, char *filename, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode_File(symbol, filename);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Print(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }

    return error_number;
}

int ZBarcode_Encode_File_and_Buffer(struct zint_symbol *symbol, char *filename, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode_File(symbol, filename);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Buffer(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }

    return error_number;
}

int ZBarcode_Encode_File_and_Buffer_Vector(struct zint_symbol *symbol, char *filename, int rotate_angle) {
    int error_number;
    int first_err;

    error_number = ZBarcode_Encode_File(symbol, filename);
    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    first_err = error_number;
    error_number = ZBarcode_Buffer_Vector(symbol, rotate_angle);
    if (error_number == 0) {
        error_number = first_err;
    }

    return error_number;
}

int ZBarcode_Version() {
    if (ZINT_VERSION_BUILD) {
        return (ZINT_VERSION_MAJOR * 10000) + (ZINT_VERSION_MINOR * 100) + ZINT_VERSION_RELEASE * 10
                + ZINT_VERSION_BUILD;
    }
    return (ZINT_VERSION_MAJOR * 10000) + (ZINT_VERSION_MINOR * 100) + ZINT_VERSION_RELEASE;
}
