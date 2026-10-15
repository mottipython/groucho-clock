/* Made by tools/make_parts.py: where each part of Groucho's face lay on the sheet,
 * in its pixels, so that the parts go back where they were drawn. */
#define SHEET_W 2048
#define SHEET_H 2048
typedef struct { int x0, y0, x1, y1; } PartBox;
static const PartBox BOX_brow_l = {1122, 501, 1454, 631};
static const PartBox BOX_brow_r = {1593, 501, 1923, 631};
static const PartBox BOX_cigar0 = {1237, 887, 1805, 1030};
static const PartBox BOX_cigar1 = {1236, 1108, 1807, 1248};
static const PartBox BOX_cigar2 = {1235, 1322, 1808, 1463};
static const PartBox BOX_cigar3 = {1172, 1619, 1501, 1961};
static const PartBox BOX_face = {75, 99, 1026, 1475};
static const PartBox BOX_glasses = {1687, 176, 1711, 203};
static const PartBox BOX_grin = {1047, 106, 1503, 446};
static const PartBox BOX_pupil = {1250, 708, 1334, 792};
static const PartBox BOX_smoke = {1764, 1591, 1874, 1944};
#define ASH_ON_CIGAR_X 198
#define ASH_ON_CIGAR_Y 5   /* where the ash was on the last cigar */
#define STACHE_ON_FACE_X 281
#define STACHE_ON_FACE_Y 624   /* where the moustache piece came from on the face */
