/* Unit tests for utilities
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * Author: Matthias Clasen, Behdad Esfahbod
 */

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "glib.h"

static void
test_unichar_validate (void)
{
  g_assert (g_unichar_validate ('j'));
  g_assert (g_unichar_validate (8356));
  g_assert (g_unichar_validate (8356));
  g_assert (g_unichar_validate (0xfdd1));
  g_assert (g_unichar_validate (917760));
  g_assert (!g_unichar_validate (0x110000));
}

static void
test_unichar_character_type (void)
{
  guint i;
  struct {
    GUnicodeType type;
    gunichar     c;
  } examples[] = {
    { G_UNICODE_CONTROL,              0x000D },
    { G_UNICODE_FORMAT,               0x200E },
     /* G_UNICODE_UNASSIGNED */
    { G_UNICODE_PRIVATE_USE,          0xE000 },
    { G_UNICODE_SURROGATE,            0xD800 },
    { G_UNICODE_LOWERCASE_LETTER,     0x0061 },
    { G_UNICODE_MODIFIER_LETTER,      0x02B0 },
    { G_UNICODE_OTHER_LETTER,         0x3400 },
    { G_UNICODE_TITLECASE_LETTER,     0x01C5 },
    { G_UNICODE_UPPERCASE_LETTER,     0xFF21 },
    { G_UNICODE_COMBINING_MARK,       0x0903 },
    { G_UNICODE_ENCLOSING_MARK,       0x20DD },
    { G_UNICODE_NON_SPACING_MARK,     0xA806 },
    { G_UNICODE_DECIMAL_NUMBER,       0xFF10 },
    { G_UNICODE_LETTER_NUMBER,        0x16EE },
    { G_UNICODE_OTHER_NUMBER,         0x17F0 },
    { G_UNICODE_CONNECT_PUNCTUATION,  0x005F },
    { G_UNICODE_DASH_PUNCTUATION,     0x058A },
    { G_UNICODE_CLOSE_PUNCTUATION,    0x0F3B },
    { G_UNICODE_FINAL_PUNCTUATION,    0x2019 },
    { G_UNICODE_INITIAL_PUNCTUATION,  0x2018 },
    { G_UNICODE_OTHER_PUNCTUATION,    0x2016 },
    { G_UNICODE_OPEN_PUNCTUATION,     0x0F3A },
    { G_UNICODE_CURRENCY_SYMBOL,      0x20A0 },
    { G_UNICODE_MODIFIER_SYMBOL,      0x309B },
    { G_UNICODE_MATH_SYMBOL,          0xFB29 },
    { G_UNICODE_OTHER_SYMBOL,         0x00A6 },
    { G_UNICODE_LINE_SEPARATOR,       0x2028 },
    { G_UNICODE_PARAGRAPH_SEPARATOR,  0x2029 },
    { G_UNICODE_SPACE_SEPARATOR,      0x202F },
  };

  for (i = 0; i < G_N_ELEMENTS (examples); i++)
    {
      g_assert_cmpint (g_unichar_type (examples[i].c), ==, examples[i].type);
    }
}

static void
test_unichar_break_type (void)
{
  guint i;
  struct {
    GUnicodeBreakType type;
    gunichar          c;
  } examples[] = {
    { G_UNICODE_BREAK_MANDATORY,           0x2028 },
    { G_UNICODE_BREAK_CARRIAGE_RETURN,     0x000D },
    { G_UNICODE_BREAK_LINE_FEED,           0x000A },
    { G_UNICODE_BREAK_COMBINING_MARK,      0x0300 },
    { G_UNICODE_BREAK_SURROGATE,           0xD800 },
    { G_UNICODE_BREAK_ZERO_WIDTH_SPACE,    0x200B },
    { G_UNICODE_BREAK_INSEPARABLE,         0x2024 },
    { G_UNICODE_BREAK_NON_BREAKING_GLUE,   0x00A0 },
    { G_UNICODE_BREAK_CONTINGENT,          0xFFFC },
    { G_UNICODE_BREAK_SPACE,               0x0020 },
    { G_UNICODE_BREAK_AFTER,               0x05BE },
    { G_UNICODE_BREAK_BEFORE,              0x02C8 },
    { G_UNICODE_BREAK_BEFORE_AND_AFTER,    0x2014 },
    { G_UNICODE_BREAK_HYPHEN,              0x002D },
    { G_UNICODE_BREAK_NON_STARTER,         0x17D6 },
    { G_UNICODE_BREAK_OPEN_PUNCTUATION,    0x0028 },
    { G_UNICODE_BREAK_CLOSE_PARANTHESIS,   0x0029 },
    { G_UNICODE_BREAK_CLOSE_PUNCTUATION,   0x007D },
    { G_UNICODE_BREAK_QUOTATION,           0x0022 },
    { G_UNICODE_BREAK_EXCLAMATION,         0x0021 },
    { G_UNICODE_BREAK_IDEOGRAPHIC,         0x2E80 },
    { G_UNICODE_BREAK_NUMERIC,             0x0030 },
    { G_UNICODE_BREAK_INFIX_SEPARATOR,     0x002C },
    { G_UNICODE_BREAK_SYMBOL,              0x002F },
    { G_UNICODE_BREAK_ALPHABETIC,          0x0023 },
    { G_UNICODE_BREAK_PREFIX,              0x0024 },
    { G_UNICODE_BREAK_POSTFIX,             0x0025 },
    { G_UNICODE_BREAK_COMPLEX_CONTEXT,     0x0E01 },
    { G_UNICODE_BREAK_AMBIGUOUS,           0x00F7 },
    { G_UNICODE_BREAK_UNKNOWN,             0xE000 },
    { G_UNICODE_BREAK_NEXT_LINE,           0x0085 },
    { G_UNICODE_BREAK_WORD_JOINER,         0x2060 },
    { G_UNICODE_BREAK_HANGUL_L_JAMO,       0x1100 },
    { G_UNICODE_BREAK_HANGUL_V_JAMO,       0x1160 },
    { G_UNICODE_BREAK_HANGUL_T_JAMO,       0x11A8 },
    { G_UNICODE_BREAK_HANGUL_LV_SYLLABLE,  0xAC00 },
    { G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE, 0xAC01 },
    { G_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER, 0x3041 },
    { G_UNICODE_BREAK_HEBREW_LETTER,                0x05D0 },
    { G_UNICODE_BREAK_REGIONAL_INDICATOR,           0x1F1F6 },
    { G_UNICODE_BREAK_EMOJI_BASE,          0x1F466 },
    { G_UNICODE_BREAK_EMOJI_MODIFIER,      0x1F3FB },
    { G_UNICODE_BREAK_ZERO_WIDTH_JOINER,   0x200D },
  };

  for (i = 0; i < G_N_ELEMENTS (examples); i++)
    {
      g_assert_cmpint (g_unichar_break_type (examples[i].c), ==, examples[i].type);
    }
}

static void
test_unichar_script (void)
{
  guint i;
  struct {
    GUnicodeScript script;
    gunichar          c;
  } examples[] = {
    { G_UNICODE_SCRIPT_COMMON,                  0x002A },
    { G_UNICODE_SCRIPT_INHERITED,               0x1CED },
    { G_UNICODE_SCRIPT_INHERITED,               0x0670 },
    { G_UNICODE_SCRIPT_ARABIC,                  0x060D },
    { G_UNICODE_SCRIPT_ARMENIAN,                0x0559 },
    { G_UNICODE_SCRIPT_BENGALI,                 0x09CD },
    { G_UNICODE_SCRIPT_BOPOMOFO,                0x31B6 },
    { G_UNICODE_SCRIPT_CHEROKEE,                0x13A2 },
    { G_UNICODE_SCRIPT_COPTIC,                  0x2CFD },
    { G_UNICODE_SCRIPT_CYRILLIC,                0x0482 },
    { G_UNICODE_SCRIPT_DESERET,                0x10401 },
    { G_UNICODE_SCRIPT_DEVANAGARI,              0x094D },
    { G_UNICODE_SCRIPT_ETHIOPIC,                0x1258 },
    { G_UNICODE_SCRIPT_GEORGIAN,                0x10FC },
    { G_UNICODE_SCRIPT_GOTHIC,                 0x10341 },
    { G_UNICODE_SCRIPT_GREEK,                   0x0375 },
    { G_UNICODE_SCRIPT_GUJARATI,                0x0A83 },
    { G_UNICODE_SCRIPT_GURMUKHI,                0x0A3C },
    { G_UNICODE_SCRIPT_HAN,                     0x3005 },
    { G_UNICODE_SCRIPT_HANGUL,                  0x1100 },
    { G_UNICODE_SCRIPT_HEBREW,                  0x05BF },
    { G_UNICODE_SCRIPT_HIRAGANA,                0x309F },
    { G_UNICODE_SCRIPT_KANNADA,                 0x0CBC },
    { G_UNICODE_SCRIPT_KATAKANA,                0x30FF },
    { G_UNICODE_SCRIPT_KHMER,                   0x17DD },
    { G_UNICODE_SCRIPT_LAO,                     0x0EDD },
    { G_UNICODE_SCRIPT_LATIN,                   0x0061 },
    { G_UNICODE_SCRIPT_MALAYALAM,               0x0D3D },
    { G_UNICODE_SCRIPT_MONGOLIAN,               0x1843 },
    { G_UNICODE_SCRIPT_MYANMAR,                 0x1031 },
    { G_UNICODE_SCRIPT_OGHAM,                   0x169C },
    { G_UNICODE_SCRIPT_OLD_ITALIC,             0x10322 },
    { G_UNICODE_SCRIPT_ORIYA,                   0x0B3C },
    { G_UNICODE_SCRIPT_RUNIC,                   0x16EF },
    { G_UNICODE_SCRIPT_SINHALA,                 0x0DBD },
    { G_UNICODE_SCRIPT_SYRIAC,                  0x0711 },
    { G_UNICODE_SCRIPT_TAMIL,                   0x0B82 },
    { G_UNICODE_SCRIPT_TELUGU,                  0x0C03 },
    { G_UNICODE_SCRIPT_THAANA,                  0x07B1 },
    { G_UNICODE_SCRIPT_THAI,                    0x0E31 },
    { G_UNICODE_SCRIPT_TIBETAN,                 0x0FD4 },
    { G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL,     0x1400 },
    { G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL,     0x1401 },
    { G_UNICODE_SCRIPT_YI,                      0xA015 },
    { G_UNICODE_SCRIPT_TAGALOG,                 0x1700 },
    { G_UNICODE_SCRIPT_HANUNOO,                 0x1720 },
    { G_UNICODE_SCRIPT_BUHID,                   0x1740 },
    { G_UNICODE_SCRIPT_TAGBANWA,                0x1760 },
    { G_UNICODE_SCRIPT_BRAILLE,                 0x2800 },
    { G_UNICODE_SCRIPT_CYPRIOT,                0x10808 },
    { G_UNICODE_SCRIPT_LIMBU,                   0x1932 },
    { G_UNICODE_SCRIPT_OSMANYA,                0x10480 },
    { G_UNICODE_SCRIPT_SHAVIAN,                0x10450 },
    { G_UNICODE_SCRIPT_LINEAR_B,               0x10000 },
    { G_UNICODE_SCRIPT_TAI_LE,                  0x1950 },
    { G_UNICODE_SCRIPT_UGARITIC,               0x1039F },
    { G_UNICODE_SCRIPT_NEW_TAI_LUE,             0x1980 },
    { G_UNICODE_SCRIPT_BUGINESE,                0x1A1F },
    { G_UNICODE_SCRIPT_GLAGOLITIC,              0x2C00 },
    { G_UNICODE_SCRIPT_TIFINAGH,                0x2D6F },
    { G_UNICODE_SCRIPT_SYLOTI_NAGRI,            0xA800 },
    { G_UNICODE_SCRIPT_OLD_PERSIAN,            0x103D0 },
    { G_UNICODE_SCRIPT_KHAROSHTHI,             0x10A3F },
    { G_UNICODE_SCRIPT_UNKNOWN,              0x1111111 },
    { G_UNICODE_SCRIPT_BALINESE,                0x1B04 },
    { G_UNICODE_SCRIPT_CUNEIFORM,              0x12000 },
    { G_UNICODE_SCRIPT_PHOENICIAN,             0x10900 },
    { G_UNICODE_SCRIPT_PHAGS_PA,                0xA840 },
    { G_UNICODE_SCRIPT_NKO,                     0x07C0 },
    { G_UNICODE_SCRIPT_KAYAH_LI,                0xA900 },
    { G_UNICODE_SCRIPT_LEPCHA,                  0x1C00 },
    { G_UNICODE_SCRIPT_REJANG,                  0xA930 },
    { G_UNICODE_SCRIPT_SUNDANESE,               0x1B80 },
    { G_UNICODE_SCRIPT_SAURASHTRA,              0xA880 },
    { G_UNICODE_SCRIPT_CHAM,                    0xAA00 },
    { G_UNICODE_SCRIPT_OL_CHIKI,                0x1C50 },
    { G_UNICODE_SCRIPT_VAI,                     0xA500 },
    { G_UNICODE_SCRIPT_CARIAN,                 0x102A0 },
    { G_UNICODE_SCRIPT_LYCIAN,                 0x10280 },
    { G_UNICODE_SCRIPT_LYDIAN,                 0x1093F },
    { G_UNICODE_SCRIPT_AVESTAN,                0x10B00 },
    { G_UNICODE_SCRIPT_BAMUM,                   0xA6A0 },
    { G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS,   0x13000 },
    { G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,       0x10840 },
    { G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,  0x10B60 },
    { G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, 0x10B40 },
    { G_UNICODE_SCRIPT_JAVANESE,                0xA980 },
    { G_UNICODE_SCRIPT_KAITHI,                 0x11082 },
    { G_UNICODE_SCRIPT_LISU,                    0xA4D0 },
    { G_UNICODE_SCRIPT_MEETEI_MAYEK,            0xABE5 },
    { G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,      0x10A60 },
    { G_UNICODE_SCRIPT_OLD_TURKIC,             0x10C00 },
    { G_UNICODE_SCRIPT_SAMARITAN,               0x0800 },
    { G_UNICODE_SCRIPT_TAI_THAM,                0x1A20 },
    { G_UNICODE_SCRIPT_TAI_VIET,                0xAA80 },
    { G_UNICODE_SCRIPT_BATAK,                   0x1BC0 },
    { G_UNICODE_SCRIPT_BRAHMI,                 0x11000 },
    { G_UNICODE_SCRIPT_MANDAIC,                 0x0840 },
    { G_UNICODE_SCRIPT_CHAKMA,                 0x11100 },
    { G_UNICODE_SCRIPT_MEROITIC_CURSIVE,       0x109A0 },
    { G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,   0x10980 },
    { G_UNICODE_SCRIPT_MIAO,                   0x16F00 },
    { G_UNICODE_SCRIPT_SHARADA,                0x11180 },
    { G_UNICODE_SCRIPT_SORA_SOMPENG,           0x110D0 },
    { G_UNICODE_SCRIPT_TAKRI,                  0x11680 },
    { G_UNICODE_SCRIPT_BASSA_VAH,              0x16AD0 },
    { G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN,     0x10530 },
    { G_UNICODE_SCRIPT_DUPLOYAN,               0x1BC00 },
    { G_UNICODE_SCRIPT_ELBASAN,                0x10500 },
    { G_UNICODE_SCRIPT_GRANTHA,                0x11301 },
    { G_UNICODE_SCRIPT_KHOJKI,                 0x11200 },
    { G_UNICODE_SCRIPT_KHUDAWADI,              0x112B0 },
    { G_UNICODE_SCRIPT_LINEAR_A,               0x10600 },
    { G_UNICODE_SCRIPT_MAHAJANI,               0x11150 },
    { G_UNICODE_SCRIPT_MANICHAEAN,             0x10AC0 },
    { G_UNICODE_SCRIPT_MENDE_KIKAKUI,          0x1E800 },
    { G_UNICODE_SCRIPT_MODI,                   0x11600 },
    { G_UNICODE_SCRIPT_MRO,                    0x16A40 },
    { G_UNICODE_SCRIPT_NABATAEAN,              0x10880 },
    { G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,      0x10A80 },
    { G_UNICODE_SCRIPT_OLD_PERMIC,             0x10350 },
    { G_UNICODE_SCRIPT_PAHAWH_HMONG,           0x16B00 },
    { G_UNICODE_SCRIPT_PALMYRENE,              0x10860 },
    { G_UNICODE_SCRIPT_PAU_CIN_HAU,            0x11AC0 },
    { G_UNICODE_SCRIPT_PSALTER_PAHLAVI,        0x10B80 },
    { G_UNICODE_SCRIPT_SIDDHAM,                0x11580 },
    { G_UNICODE_SCRIPT_TIRHUTA,                0x11480 },
    { G_UNICODE_SCRIPT_WARANG_CITI,            0x118A0 },
    { G_UNICODE_SCRIPT_CHEROKEE,               0x0AB71 },
    { G_UNICODE_SCRIPT_HATRAN,                 0x108E0 },
    { G_UNICODE_SCRIPT_OLD_HUNGARIAN,          0x10C80 },
    { G_UNICODE_SCRIPT_MULTANI,                0x11280 },
    { G_UNICODE_SCRIPT_AHOM,                   0x11700 },
    { G_UNICODE_SCRIPT_CUNEIFORM,              0x12480 },
    { G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS,  0x14400 },
    { G_UNICODE_SCRIPT_SIGNWRITING,            0x1D800 },
    { G_UNICODE_SCRIPT_ADLAM,                  0x1E900 },
    { G_UNICODE_SCRIPT_BHAIKSUKI,              0x11C00 },
    { G_UNICODE_SCRIPT_MARCHEN,                0x11C70 },
    { G_UNICODE_SCRIPT_NEWA,                   0x11400 },
    { G_UNICODE_SCRIPT_OSAGE,                  0x104B0 },
    { G_UNICODE_SCRIPT_TANGUT,                 0x16FE0 },
    { G_UNICODE_SCRIPT_MASARAM_GONDI,          0x11D00 },
    { G_UNICODE_SCRIPT_NUSHU,                  0x1B170 },
    { G_UNICODE_SCRIPT_SOYOMBO,                0x11A50 },
    { G_UNICODE_SCRIPT_ZANABAZAR_SQUARE,       0x11A00 },
  };
  for (i = 0; i < G_N_ELEMENTS (examples); i++)
    g_assert_cmpint (g_unichar_get_script (examples[i].c), ==, examples[i].script);
}

static void
test_combining_class (void)
{
  guint i;
  struct {
    gint class;
    gunichar          c;
  } examples[] = {
    {   0, 0x0020 },
    {   1, 0x0334 },
    {   7, 0x093C },
    {   8, 0x3099 },
    {   9, 0x094D },
    {  10, 0x05B0 },
    {  11, 0x05B1 },
    {  12, 0x05B2 },
    {  13, 0x05B3 },
    {  14, 0x05B4 },
    {  15, 0x05B5 },
    {  16, 0x05B6 },
    {  17, 0x05B7 },
    {  18, 0x05B8 },
    {  19, 0x05B9 },
    {  20, 0x05BB },
    {  21, 0x05BC },
    {  22, 0x05BD },
    {  23, 0x05BF },
    {  24, 0x05C1 },
    {  25, 0x05C2 },
    {  26, 0xFB1E },
    {  27, 0x064B },
    {  28, 0x064C },
    {  29, 0x064D },
    /* ... */
    { 228, 0x05AE },
    { 230, 0x0300 },
    { 232, 0x302C },
    { 233, 0x0362 },
    { 234, 0x0360 },
    { 234, 0x1DCD },
    { 240, 0x0345 }
  };
  for (i = 0; i < G_N_ELEMENTS (examples); i++)
    {
      g_assert_cmpint (g_unichar_combining_class (examples[i].c), ==, examples[i].class);
    }
}

static void
test_mirror (void)
{
  gunichar mirror;

  g_assert (g_unichar_get_mirror_char ('(', &mirror));
  g_assert_cmpint (mirror, ==, ')');
  g_assert (g_unichar_get_mirror_char (')', &mirror));
  g_assert_cmpint (mirror, ==, '(');
  g_assert (g_unichar_get_mirror_char ('{', &mirror));
  g_assert_cmpint (mirror, ==, '}');
  g_assert (g_unichar_get_mirror_char ('}', &mirror));
  g_assert_cmpint (mirror, ==, '{');
  g_assert (g_unichar_get_mirror_char (0x208D, &mirror));
  g_assert_cmpint (mirror, ==, 0x208E);
  g_assert (g_unichar_get_mirror_char (0x208E, &mirror));
  g_assert_cmpint (mirror, ==, 0x208D);
  g_assert (!g_unichar_get_mirror_char ('a', &mirror));
}

static void
test_mark (void)
{
  g_assert (g_unichar_ismark (0x0903));
  g_assert (g_unichar_ismark (0x20DD));
  g_assert (g_unichar_ismark (0xA806));
  g_assert (!g_unichar_ismark ('a'));
}

static void
test_zerowidth (void)
{
  g_assert (!g_unichar_iszerowidth (0x00AD));
  g_assert (!g_unichar_iszerowidth (0x00AD));
  g_assert (!g_unichar_iszerowidth (0x115F));
  g_assert (g_unichar_iszerowidth (0x1160));
  g_assert (g_unichar_iszerowidth (0x11AA));
  g_assert (g_unichar_iszerowidth (0x11FF));
  g_assert (!g_unichar_iszerowidth (0x1200));
  g_assert (g_unichar_iszerowidth (0x200B));
  g_assert (g_unichar_iszerowidth (0x591));
}

static void
test_title (void)
{
  g_assert (g_unichar_istitle (0x01c5));
  g_assert (g_unichar_istitle (0x1f88));
  g_assert (g_unichar_istitle (0x1fcc));
  g_assert (!g_unichar_istitle ('a'));
  g_assert (!g_unichar_istitle ('A'));

  g_assert_cmphex (g_unichar_totitle (0x01c6), ==, 0x01c5);
  g_assert_cmphex (g_unichar_totitle (0x01c4), ==, 0x01c5);
  g_assert_cmphex (g_unichar_totitle (0x01c5), ==, 0x01c5);
  g_assert_cmphex (g_unichar_totitle (0x1f80), ==, 0x1f88);
  g_assert_cmphex (g_unichar_totitle (0x1f88), ==, 0x1f88);
  g_assert_cmphex (g_unichar_totitle ('a'), ==, 'A');
  g_assert_cmphex (g_unichar_totitle ('A'), ==, 'A');
}

static void
test_cases (void)
{
  g_assert_cmphex (g_unichar_toupper ('a'), ==, 'A');
  g_assert_cmphex (g_unichar_toupper ('A'), ==, 'A');
  g_assert_cmphex (g_unichar_toupper (0x01C5), ==, 0x01C4);
  g_assert_cmphex (g_unichar_toupper (0x01C6), ==, 0x01C4);
  g_assert_cmphex (g_unichar_tolower ('A'), ==, 'a');
  g_assert_cmphex (g_unichar_tolower ('a'), ==, 'a');
  g_assert_cmphex (g_unichar_tolower (0x01C4), ==, 0x01C6);
  g_assert_cmphex (g_unichar_tolower (0x01C5), ==, 0x01C6);
  g_assert_cmphex (g_unichar_tolower (0x1F8A), ==, 0x1F82);
  g_assert_cmphex (g_unichar_totitle (0x1F8A), ==, 0x1F8A);
  g_assert_cmphex (g_unichar_toupper (0x1F8A), ==, 0x1F8A);
  g_assert_cmphex (g_unichar_tolower (0x1FB2), ==, 0x1FB2);
  g_assert_cmphex (g_unichar_toupper (0x1FB2), ==, 0x1FB2);
}

static void
test_defined (void)
{
  g_assert (g_unichar_isdefined (0x0903));
  g_assert (g_unichar_isdefined (0x20DD));
  g_assert (g_unichar_isdefined (0x20BA));
  g_assert (g_unichar_isdefined (0xA806));
  g_assert (g_unichar_isdefined ('a'));
  g_assert (!g_unichar_isdefined (0x10C49));
  g_assert (!g_unichar_isdefined (0x169D));
}

static void
test_wide (void)
{
  guint i;
  struct {
    gunichar c;
    enum {
      NOT_WIDE,
      WIDE_CJK,
      WIDE
    } wide;
  } examples[] = {
    /* Neutral */
    {   0x0000, NOT_WIDE },
    {   0x0483, NOT_WIDE },
    {   0x0641, NOT_WIDE },
    {   0xFFFC, NOT_WIDE },
    {  0x10000, NOT_WIDE },
    {  0xE0001, NOT_WIDE },
    {  0x2FFFE, NOT_WIDE },
    {  0x3FFFE, NOT_WIDE },

    /* Narrow */
    {   0x0020, NOT_WIDE },
    {   0x0041, NOT_WIDE },
    {   0x27E6, NOT_WIDE },

    /* Halfwidth */
    {   0x20A9, NOT_WIDE },
    {   0xFF61, NOT_WIDE },
    {   0xFF69, NOT_WIDE },
    {   0xFFEE, NOT_WIDE },

    /* Ambiguous */
    {   0x00A1, WIDE_CJK },
    {   0x00BE, WIDE_CJK },
    {   0x02DD, WIDE_CJK },
    {   0x2020, WIDE_CJK },
    {   0xFFFD, WIDE_CJK },
    {   0x00A1, WIDE_CJK },
    {  0x1F100, WIDE_CJK },
    {  0xE0100, WIDE_CJK },
    { 0x100000, WIDE_CJK },
    { 0x10FFFD, WIDE_CJK },

    /* Fullwidth */
    {   0x3000, WIDE },
    {   0xFF60, WIDE },

    /* Wide */
    {   0x2329, WIDE },
    {   0x3001, WIDE },
    {   0xFE69, WIDE },
    {  0x30000, WIDE },
    {  0x3FFFD, WIDE },

    /* Default Wide blocks */
    {   0x4DBF, WIDE },
    {   0x9FFF, WIDE },
    {   0xFAFF, WIDE },
    {  0x2A6DF, WIDE },
    {  0x2B73F, WIDE },
    {  0x2B81F, WIDE },
    {  0x2FA1F, WIDE },

    /* Uniode-5.2 character additions */
    /* Wide */
    {   0x115F, WIDE },

    /* Uniode-6.0 character additions */
    /* Wide */
    {  0x2B740, WIDE },
    {  0x1B000, WIDE },

    { 0x111111, NOT_WIDE }
  };

  for (i = 0; i < G_N_ELEMENTS (examples); i++)
    {
      g_assert_cmpint (g_unichar_iswide (examples[i].c), ==, (examples[i].wide == WIDE));
      g_assert_cmpint (g_unichar_iswide_cjk (examples[i].c), ==, (examples[i].wide != NOT_WIDE));
    }
};

static void
test_compose (void)
{
  gunichar ch;

  /* Not composable */
  g_assert (!g_unichar_compose (0x0041, 0x0042, &ch) && ch == 0);
  g_assert (!g_unichar_compose (0x0041, 0, &ch) && ch == 0);
  g_assert (!g_unichar_compose (0x0066, 0x0069, &ch) && ch == 0);

  /* Tricky non-composable */
  g_assert (!g_unichar_compose (0x0308, 0x0301, &ch) && ch == 0); /* !0x0344 */
  g_assert (!g_unichar_compose (0x0F71, 0x0F72, &ch) && ch == 0); /* !0x0F73 */

  /* Singletons should not compose */
  g_assert (!g_unichar_compose (0x212B, 0, &ch) && ch == 0);
  g_assert (!g_unichar_compose (0x00C5, 0, &ch) && ch == 0);
  g_assert (!g_unichar_compose (0x2126, 0, &ch) && ch == 0);
  g_assert (!g_unichar_compose (0x03A9, 0, &ch) && ch == 0);

  /* Pairs */
  g_assert (g_unichar_compose (0x0041, 0x030A, &ch) && ch == 0x00C5);
  g_assert (g_unichar_compose (0x006F, 0x0302, &ch) && ch == 0x00F4);
  g_assert (g_unichar_compose (0x1E63, 0x0307, &ch) && ch == 0x1E69);
  g_assert (g_unichar_compose (0x0073, 0x0323, &ch) && ch == 0x1E63);
  g_assert (g_unichar_compose (0x0064, 0x0307, &ch) && ch == 0x1E0B);
  g_assert (g_unichar_compose (0x0064, 0x0323, &ch) && ch == 0x1E0D);

  /* Hangul */
  g_assert (g_unichar_compose (0xD4CC, 0x11B6, &ch) && ch == 0xD4DB);
  g_assert (g_unichar_compose (0x1111, 0x1171, &ch) && ch == 0xD4CC);
  g_assert (g_unichar_compose (0xCE20, 0x11B8, &ch) && ch == 0xCE31);
  g_assert (g_unichar_compose (0x110E, 0x1173, &ch) && ch == 0xCE20);
}

static void
test_decompose (void)
{
  gunichar a, b;

  /* Not decomposable */
  g_assert (!g_unichar_decompose (0x0041, &a, &b) && a == 0x0041 && b == 0);
  g_assert (!g_unichar_decompose (0xFB01, &a, &b) && a == 0xFB01 && b == 0);

  /* Singletons */
  g_assert (g_unichar_decompose (0x212B, &a, &b) && a == 0x00C5 && b == 0);
  g_assert (g_unichar_decompose (0x2126, &a, &b) && a == 0x03A9 && b == 0);

  /* Tricky pairs */
  g_assert (g_unichar_decompose (0x0344, &a, &b) && a == 0x0308 && b == 0x0301);
  g_assert (g_unichar_decompose (0x0F73, &a, &b) && a == 0x0F71 && b == 0x0F72);

  /* Pairs */
  g_assert (g_unichar_decompose (0x00C5, &a, &b) && a == 0x0041 && b == 0x030A);
  g_assert (g_unichar_decompose (0x00F4, &a, &b) && a == 0x006F && b == 0x0302);
  g_assert (g_unichar_decompose (0x1E69, &a, &b) && a == 0x1E63 && b == 0x0307);
  g_assert (g_unichar_decompose (0x1E63, &a, &b) && a == 0x0073 && b == 0x0323);
  g_assert (g_unichar_decompose (0x1E0B, &a, &b) && a == 0x0064 && b == 0x0307);
  g_assert (g_unichar_decompose (0x1E0D, &a, &b) && a == 0x0064 && b == 0x0323);

  /* Hangul */
  g_assert (g_unichar_decompose (0xD4DB, &a, &b) && a == 0xD4CC && b == 0x11B6);
  g_assert (g_unichar_decompose (0xD4CC, &a, &b) && a == 0x1111 && b == 0x1171);
  g_assert (g_unichar_decompose (0xCE31, &a, &b) && a == 0xCE20 && b == 0x11B8);
  g_assert (g_unichar_decompose (0xCE20, &a, &b) && a == 0x110E && b == 0x1173);
}

static void
test_fully_decompose_canonical (void)
{
  gunichar decomp[5];
  gsize len;

#define TEST_DECOMP(ch, expected_len, a, b, c, d) \
  len = g_unichar_fully_decompose (ch, FALSE, decomp, G_N_ELEMENTS (decomp)); \
  g_assert_cmpint (expected_len, ==, len); \
  if (expected_len >= 1) g_assert_cmphex (decomp[0], ==, a); \
  if (expected_len >= 2) g_assert_cmphex (decomp[1], ==, b); \
  if (expected_len >= 3) g_assert_cmphex (decomp[2], ==, c); \
  if (expected_len >= 4) g_assert_cmphex (decomp[3], ==, d); \

#define TEST0(ch)		TEST_DECOMP (ch, 1, ch, 0, 0, 0)
#define TEST1(ch, a)		TEST_DECOMP (ch, 1, a, 0, 0, 0)
#define TEST2(ch, a, b)		TEST_DECOMP (ch, 2, a, b, 0, 0)
#define TEST3(ch, a, b, c)	TEST_DECOMP (ch, 3, a, b, c, 0)
#define TEST4(ch, a, b, c, d)	TEST_DECOMP (ch, 4, a, b, c, d)

  /* Not decomposable */
  TEST0 (0x0041);
  TEST0 (0xFB01);

  /* Singletons */
  TEST2 (0x212B, 0x0041, 0x030A);
  TEST1 (0x2126, 0x03A9);

  /* Tricky pairs */
  TEST2 (0x0344, 0x0308, 0x0301);
  TEST2 (0x0F73, 0x0F71, 0x0F72);

  /* General */
  TEST2 (0x00C5, 0x0041, 0x030A);
  TEST2 (0x00F4, 0x006F, 0x0302);
  TEST3 (0x1E69, 0x0073, 0x0323, 0x0307);
  TEST2 (0x1E63, 0x0073, 0x0323);
  TEST2 (0x1E0B, 0x0064, 0x0307);
  TEST2 (0x1E0D, 0x0064, 0x0323);

  /* Hangul */
  TEST3 (0xD4DB, 0x1111, 0x1171, 0x11B6);
  TEST2 (0xD4CC, 0x1111, 0x1171);
  TEST3 (0xCE31, 0x110E, 0x1173, 0x11B8);
  TEST2 (0xCE20, 0x110E, 0x1173);

#undef TEST_DECOMP
}

static void
test_canonical_decomposition (void)
{
  gunichar *decomp;
  gsize len;

#define TEST_DECOMP(ch, expected_len, a, b, c, d) \
  decomp = g_unicode_canonical_decomposition (ch, &len); \
  g_assert_cmpint (expected_len, ==, len); \
  if (expected_len >= 1) g_assert_cmphex (decomp[0], ==, a); \
  if (expected_len >= 2) g_assert_cmphex (decomp[1], ==, b); \
  if (expected_len >= 3) g_assert_cmphex (decomp[2], ==, c); \
  if (expected_len >= 4) g_assert_cmphex (decomp[3], ==, d); \
  g_free (decomp);

#define TEST0(ch)		TEST_DECOMP (ch, 1, ch, 0, 0, 0)
#define TEST1(ch, a)		TEST_DECOMP (ch, 1, a, 0, 0, 0)
#define TEST2(ch, a, b)		TEST_DECOMP (ch, 2, a, b, 0, 0)
#define TEST3(ch, a, b, c)	TEST_DECOMP (ch, 3, a, b, c, 0)
#define TEST4(ch, a, b, c, d)	TEST_DECOMP (ch, 4, a, b, c, d)

  /* Not decomposable */
  TEST0 (0x0041);
  TEST0 (0xFB01);

  /* Singletons */
  TEST2 (0x212B, 0x0041, 0x030A);
  TEST1 (0x2126, 0x03A9);

  /* Tricky pairs */
  TEST2 (0x0344, 0x0308, 0x0301);
  TEST2 (0x0F73, 0x0F71, 0x0F72);

  /* General */
  TEST2 (0x00C5, 0x0041, 0x030A);
  TEST2 (0x00F4, 0x006F, 0x0302);
  TEST3 (0x1E69, 0x0073, 0x0323, 0x0307);
  TEST2 (0x1E63, 0x0073, 0x0323);
  TEST2 (0x1E0B, 0x0064, 0x0307);
  TEST2 (0x1E0D, 0x0064, 0x0323);

  /* Hangul */
  TEST3 (0xD4DB, 0x1111, 0x1171, 0x11B6);
  TEST2 (0xD4CC, 0x1111, 0x1171);
  TEST3 (0xCE31, 0x110E, 0x1173, 0x11B8);
  TEST2 (0xCE20, 0x110E, 0x1173);

#undef TEST_DECOMP
}

static void
test_decompose_tail (void)
{
  gunichar ch, a, b, c, d;

  /* Test that whenever a char ch decomposes into a and b, b itself
   * won't decompose any further. */

  for (ch = 0; ch < 0x110000; ch++)
    if (g_unichar_decompose (ch, &a, &b))
      g_assert (!g_unichar_decompose (b, &c, &d));
    else
      {
        g_assert_cmpuint (a, ==, ch);
        g_assert_cmpuint (b, ==, 0);
      }
}

static void
test_fully_decompose_len (void)
{
  gunichar ch;

  /* Test that all canonical decompositions are at most 4 in length,
   * and compatibility decompositions are at most 18 in length.
   */

  for (ch = 0; ch < 0x110000; ch++) {
    g_assert_cmpint (g_unichar_fully_decompose (ch, FALSE, NULL, 0), <=, 4);
    g_assert_cmpint (g_unichar_fully_decompose (ch, TRUE,  NULL, 0), <=, 18);
  }
}

static void
test_iso15924 (void)
{
  const struct {
    GUnicodeScript script;
    char four_letter_code[5];
  } data[] = {
    { G_UNICODE_SCRIPT_COMMON,             "Zyyy" },
    { G_UNICODE_SCRIPT_INHERITED,          "Zinh" },
    { G_UNICODE_SCRIPT_ARABIC,             "Arab" },
    { G_UNICODE_SCRIPT_ARMENIAN,           "Armn" },
    { G_UNICODE_SCRIPT_BENGALI,            "Beng" },
    { G_UNICODE_SCRIPT_BOPOMOFO,           "Bopo" },
    { G_UNICODE_SCRIPT_CHEROKEE,           "Cher" },
    { G_UNICODE_SCRIPT_COPTIC,             "Copt" },
    { G_UNICODE_SCRIPT_CYRILLIC,           "Cyrl" },
    { G_UNICODE_SCRIPT_DESERET,            "Dsrt" },
    { G_UNICODE_SCRIPT_DEVANAGARI,         "Deva" },
    { G_UNICODE_SCRIPT_ETHIOPIC,           "Ethi" },
    { G_UNICODE_SCRIPT_GEORGIAN,           "Geor" },
    { G_UNICODE_SCRIPT_GOTHIC,             "Goth" },
    { G_UNICODE_SCRIPT_GREEK,              "Grek" },
    { G_UNICODE_SCRIPT_GUJARATI,           "Gujr" },
    { G_UNICODE_SCRIPT_GURMUKHI,           "Guru" },
    { G_UNICODE_SCRIPT_HAN,                "Hani" },
    { G_UNICODE_SCRIPT_HANGUL,             "Hang" },
    { G_UNICODE_SCRIPT_HEBREW,             "Hebr" },
    { G_UNICODE_SCRIPT_HIRAGANA,           "Hira" },
    { G_UNICODE_SCRIPT_KANNADA,            "Knda" },
    { G_UNICODE_SCRIPT_KATAKANA,           "Kana" },
    { G_UNICODE_SCRIPT_KHMER,              "Khmr" },
    { G_UNICODE_SCRIPT_LAO,                "Laoo" },
    { G_UNICODE_SCRIPT_LATIN,              "Latn" },
    { G_UNICODE_SCRIPT_MALAYALAM,          "Mlym" },
    { G_UNICODE_SCRIPT_MONGOLIAN,          "Mong" },
    { G_UNICODE_SCRIPT_MYANMAR,            "Mymr" },
    { G_UNICODE_SCRIPT_OGHAM,              "Ogam" },
    { G_UNICODE_SCRIPT_OLD_ITALIC,         "Ital" },
    { G_UNICODE_SCRIPT_ORIYA,              "Orya" },
    { G_UNICODE_SCRIPT_RUNIC,              "Runr" },
    { G_UNICODE_SCRIPT_SINHALA,            "Sinh" },
    { G_UNICODE_SCRIPT_SYRIAC,             "Syrc" },
    { G_UNICODE_SCRIPT_TAMIL,              "Taml" },
    { G_UNICODE_SCRIPT_TELUGU,             "Telu" },
    { G_UNICODE_SCRIPT_THAANA,             "Thaa" },
    { G_UNICODE_SCRIPT_THAI,               "Thai" },
    { G_UNICODE_SCRIPT_TIBETAN,            "Tibt" },
    { G_UNICODE_SCRIPT_CANADIAN_ABORIGINAL, "Cans" },
    { G_UNICODE_SCRIPT_YI,                 "Yiii" },
    { G_UNICODE_SCRIPT_TAGALOG,            "Tglg" },
    { G_UNICODE_SCRIPT_HANUNOO,            "Hano" },
    { G_UNICODE_SCRIPT_BUHID,              "Buhd" },
    { G_UNICODE_SCRIPT_TAGBANWA,           "Tagb" },

    /* Unicode-4.0 additions */
    { G_UNICODE_SCRIPT_BRAILLE,            "Brai" },
    { G_UNICODE_SCRIPT_CYPRIOT,            "Cprt" },
    { G_UNICODE_SCRIPT_LIMBU,              "Limb" },
    { G_UNICODE_SCRIPT_OSMANYA,            "Osma" },
    { G_UNICODE_SCRIPT_SHAVIAN,            "Shaw" },
    { G_UNICODE_SCRIPT_LINEAR_B,           "Linb" },
    { G_UNICODE_SCRIPT_TAI_LE,             "Tale" },
    { G_UNICODE_SCRIPT_UGARITIC,           "Ugar" },

    /* Unicode-4.1 additions */
    { G_UNICODE_SCRIPT_NEW_TAI_LUE,        "Talu" },
    { G_UNICODE_SCRIPT_BUGINESE,           "Bugi" },
    { G_UNICODE_SCRIPT_GLAGOLITIC,         "Glag" },
    { G_UNICODE_SCRIPT_TIFINAGH,           "Tfng" },
    { G_UNICODE_SCRIPT_SYLOTI_NAGRI,       "Sylo" },
    { G_UNICODE_SCRIPT_OLD_PERSIAN,        "Xpeo" },
    { G_UNICODE_SCRIPT_KHAROSHTHI,         "Khar" },

    /* Unicode-5.0 additions */
    { G_UNICODE_SCRIPT_UNKNOWN,            "Zzzz" },
    { G_UNICODE_SCRIPT_BALINESE,           "Bali" },
    { G_UNICODE_SCRIPT_CUNEIFORM,          "Xsux" },
    { G_UNICODE_SCRIPT_PHOENICIAN,         "Phnx" },
    { G_UNICODE_SCRIPT_PHAGS_PA,           "Phag" },
    { G_UNICODE_SCRIPT_NKO,                "Nkoo" },

    /* Unicode-5.1 additions */
    { G_UNICODE_SCRIPT_KAYAH_LI,           "Kali" },
    { G_UNICODE_SCRIPT_LEPCHA,             "Lepc" },
    { G_UNICODE_SCRIPT_REJANG,             "Rjng" },
    { G_UNICODE_SCRIPT_SUNDANESE,          "Sund" },
    { G_UNICODE_SCRIPT_SAURASHTRA,         "Saur" },
    { G_UNICODE_SCRIPT_CHAM,               "Cham" },
    { G_UNICODE_SCRIPT_OL_CHIKI,           "Olck" },
    { G_UNICODE_SCRIPT_VAI,                "Vaii" },
    { G_UNICODE_SCRIPT_CARIAN,             "Cari" },
    { G_UNICODE_SCRIPT_LYCIAN,             "Lyci" },
    { G_UNICODE_SCRIPT_LYDIAN,             "Lydi" },

    /* Unicode-5.2 additions */
    { G_UNICODE_SCRIPT_AVESTAN,                "Avst" },
    { G_UNICODE_SCRIPT_BAMUM,                  "Bamu" },
    { G_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS,   "Egyp" },
    { G_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,       "Armi" },
    { G_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,  "Phli" },
    { G_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, "Prti" },
    { G_UNICODE_SCRIPT_JAVANESE,               "Java" },
    { G_UNICODE_SCRIPT_KAITHI,                 "Kthi" },
    { G_UNICODE_SCRIPT_LISU,                   "Lisu" },
    { G_UNICODE_SCRIPT_MEETEI_MAYEK,           "Mtei" },
    { G_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,      "Sarb" },
    { G_UNICODE_SCRIPT_OLD_TURKIC,             "Orkh" },
    { G_UNICODE_SCRIPT_SAMARITAN,              "Samr" },
    { G_UNICODE_SCRIPT_TAI_THAM,               "Lana" },
    { G_UNICODE_SCRIPT_TAI_VIET,               "Tavt" },

    /* Unicode-6.0 additions */
    { G_UNICODE_SCRIPT_BATAK,                  "Batk" },
    { G_UNICODE_SCRIPT_BRAHMI,                 "Brah" },
    { G_UNICODE_SCRIPT_MANDAIC,                "Mand" },

    /* Unicode-6.1 additions */
    { G_UNICODE_SCRIPT_CHAKMA,                 "Cakm" },
    { G_UNICODE_SCRIPT_MEROITIC_CURSIVE,       "Merc" },
    { G_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,   "Mero" },
    { G_UNICODE_SCRIPT_MIAO,                   "Plrd" },
    { G_UNICODE_SCRIPT_SHARADA,                "Shrd" },
    { G_UNICODE_SCRIPT_SORA_SOMPENG,           "Sora" },
    { G_UNICODE_SCRIPT_TAKRI,                  "Takr" },

    /* Unicode 7.0 additions */
    { G_UNICODE_SCRIPT_BASSA_VAH,              "Bass" },
    { G_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN,     "Aghb" },
    { G_UNICODE_SCRIPT_DUPLOYAN,               "Dupl" },
    { G_UNICODE_SCRIPT_ELBASAN,                "Elba" },
    { G_UNICODE_SCRIPT_GRANTHA,                "Gran" },
    { G_UNICODE_SCRIPT_KHOJKI,                 "Khoj" },
    { G_UNICODE_SCRIPT_KHUDAWADI,              "Sind" },
    { G_UNICODE_SCRIPT_LINEAR_A,               "Lina" },
    { G_UNICODE_SCRIPT_MAHAJANI,               "Mahj" },
    { G_UNICODE_SCRIPT_MANICHAEAN,             "Manu" },
    { G_UNICODE_SCRIPT_MENDE_KIKAKUI,          "Mend" },
    { G_UNICODE_SCRIPT_MODI,                   "Modi" },
    { G_UNICODE_SCRIPT_MRO,                    "Mroo" },
    { G_UNICODE_SCRIPT_NABATAEAN,              "Nbat" },
    { G_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,      "Narb" },
    { G_UNICODE_SCRIPT_OLD_PERMIC,             "Perm" },
    { G_UNICODE_SCRIPT_PAHAWH_HMONG,           "Hmng" },
    { G_UNICODE_SCRIPT_PALMYRENE,              "Palm" },
    { G_UNICODE_SCRIPT_PAU_CIN_HAU,            "Pauc" },
    { G_UNICODE_SCRIPT_PSALTER_PAHLAVI,        "Phlp" },
    { G_UNICODE_SCRIPT_SIDDHAM,                "Sidd" },
    { G_UNICODE_SCRIPT_TIRHUTA,                "Tirh" },
    { G_UNICODE_SCRIPT_WARANG_CITI,            "Wara" },

    /* Unicode 8.0 additions */
    { G_UNICODE_SCRIPT_AHOM,                   "Ahom" },
    { G_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS,  "Hluw" },
    { G_UNICODE_SCRIPT_HATRAN,                 "Hatr" },
    { G_UNICODE_SCRIPT_MULTANI,                "Mult" },
    { G_UNICODE_SCRIPT_OLD_HUNGARIAN,          "Hung" },
    { G_UNICODE_SCRIPT_SIGNWRITING,            "Sgnw" },

    /* Unicode 9.0 additions */
    { G_UNICODE_SCRIPT_ADLAM,                  "Adlm" },
    { G_UNICODE_SCRIPT_BHAIKSUKI,              "Bhks" },
    { G_UNICODE_SCRIPT_MARCHEN,                "Marc" },
    { G_UNICODE_SCRIPT_NEWA,                   "Newa" },
    { G_UNICODE_SCRIPT_OSAGE,                  "Osge" },
    { G_UNICODE_SCRIPT_TANGUT,                 "Tang" },

    /* Unicode 10.0 additions */
    { G_UNICODE_SCRIPT_MASARAM_GONDI,          "Gonm" },
    { G_UNICODE_SCRIPT_NUSHU,                  "Nshu" },
    { G_UNICODE_SCRIPT_SOYOMBO,                "Soyo" },
    { G_UNICODE_SCRIPT_ZANABAZAR_SQUARE,       "Zanb" },
  };
  guint i;

  g_assert_cmphex (0, ==, g_unicode_script_to_iso15924 (G_UNICODE_SCRIPT_INVALID_CODE));
  g_assert_cmphex (0x5A7A7A7A, ==, g_unicode_script_to_iso15924 (1000));
  g_assert_cmphex (0x41726162, ==, g_unicode_script_to_iso15924 (G_UNICODE_SCRIPT_ARABIC));

  g_assert_cmphex (G_UNICODE_SCRIPT_INVALID_CODE, ==, g_unicode_script_from_iso15924 (0));
  g_assert_cmphex (G_UNICODE_SCRIPT_UNKNOWN, ==, g_unicode_script_from_iso15924 (0x12345678));

#define PACK(a,b,c,d) ((guint32)((((guint8)(a))<<24)|(((guint8)(b))<<16)|(((guint8)(c))<<8)|((guint8)(d))))

  for (i = 0; i < G_N_ELEMENTS (data); i++)
    {
      guint32 code = PACK (data[i].four_letter_code[0],
                           data[i].four_letter_code[1],
                           data[i].four_letter_code[2],
                           data[i].four_letter_code[3]);

      g_assert_cmphex (g_unicode_script_to_iso15924 (data[i].script), ==, code);
      g_assert_cmpint (g_unicode_script_from_iso15924 (code), ==, data[i].script);
    }

#undef PACK
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/unicode/validate", test_unichar_validate);
  g_test_add_func ("/unicode/character-type", test_unichar_character_type);
  g_test_add_func ("/unicode/break-type", test_unichar_break_type);
  g_test_add_func ("/unicode/script", test_unichar_script);
  g_test_add_func ("/unicode/combining-class", test_combining_class);
  g_test_add_func ("/unicode/mirror", test_mirror);
  g_test_add_func ("/unicode/mark", test_mark);
  g_test_add_func ("/unicode/title", test_title);
  g_test_add_func ("/unicode/zero-width", test_zerowidth);
  g_test_add_func ("/unicode/defined", test_defined);
  g_test_add_func ("/unicode/wide", test_wide);
  g_test_add_func ("/unicode/compose", test_compose);
  g_test_add_func ("/unicode/decompose", test_decompose);
  g_test_add_func ("/unicode/fully-decompose-canonical", test_fully_decompose_canonical);
  g_test_add_func ("/unicode/canonical-decomposition", test_canonical_decomposition);
  g_test_add_func ("/unicode/decompose-tail", test_decompose_tail);
  g_test_add_func ("/unicode/fully-decompose-len", test_fully_decompose_len);
  g_test_add_func ("/unicode/iso15924", test_iso15924);
  g_test_add_func ("/unicode/cases", test_cases);

  return g_test_run();
}
