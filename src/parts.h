/* Made by tools/make_parts.py: where each part of Groucho's face lay on the sheet,
 * in its pixels, so that the parts go back where they were drawn. */
#define SHEET_W 2048
#define SHEET_H 2048
typedef struct { int x0, y0, x1, y1; } PartBox;
static const PartBox BOX_brow_l = {1143, 486, 1460, 633};
static const PartBox BOX_brow_r = {1589, 486, 1905, 633};
static const PartBox BOX_cigar0 = {1236, 886, 1805, 1030};
static const PartBox BOX_cigar1 = {1236, 1108, 1808, 1250};
static const PartBox BOX_cigar2 = {1235, 1321, 1809, 1464};
static const PartBox BOX_cigar3 = {1172, 1618, 1502, 1961};
static const PartBox BOX_face = {73, 97, 1029, 1477};
static const PartBox BOX_glasses = {1517, 150, 1994, 335};
static const PartBox BOX_grin = {1046, 105, 1505, 449};
static const PartBox BOX_pupil = {1249, 707, 1335, 794};
static const PartBox BOX_smoke = {1762, 1581, 1875, 1948};
