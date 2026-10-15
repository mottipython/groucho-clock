/*
 * groucho-clock: a cartoon screensaver clock. Groucho Marx tells the time with his
 * eyes: the lenses of his glasses are two clock faces and his pupils are the hands,
 * the left on the hour and the right on the minutes. His eyebrows waggle every
 * minute, his cigar burns down through the hour, and on the hour he says something.
 * Now and then his brother Gummo peeks in.
 *
 *   groucho-clock            full screen on every display; any key or mouse movement quits
 *   groucho-clock --window   in a window; Esc or Q quits
 *   GrouchoClock.scr /s      Windows screensaver conventions
 */
#include <SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The pictures (drawn with Gemini, cut by tools/make_parts.py) are built into the
 * program as PNG and JPEG files and decoded at start-up. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS /* else Windows builds need libwinpthread-1.dll */
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if defined(__APPLE__)
/* Mach-O: its own section names, and C names carry a leading underscore */
#define ART_SECTION ".section __DATA,__const\n"
#define ART_SYM "_art_"
#elif defined(_WIN32)
#define ART_SECTION ".section .rdata,\"dr\"\n"
#define ART_SYM "art_"
#else
#define ART_SECTION ".section .rodata\n"
#define ART_SYM "art_"
#endif
#define ART(name, file) \
    __asm__(ART_SECTION ".globl " ART_SYM #name "\n" ART_SYM #name ":\n.incbin \"assets/art/" file "\"\n" \
            ".globl " ART_SYM #name "_end\n" ART_SYM #name "_end:\n.text\n"); \
    extern const unsigned char art_##name[], art_##name##_end[];
/* Every picture, by name and file (src/art.h). The figures are drawn to one scale:
 * Groucho stands 480 high, and the others at their heights beside him, each with
 * its feet at the middle of the bottom edge. */
#include "art.h"
#include "parts.h"
#define X(name, file) ART(name, file)
SPRITES
#undef X
#define X(name, file) SP_##name,
enum { SPRITES NSPRITE };
#undef X
#define X(name, file) {art_##name, art_##name##_end},
static struct { const unsigned char *file, *end; int w, h; unsigned char *px; } sprite[NSPRITE] = { SPRITES };
#undef X
static SDL_Texture **cur_tex; /* the pictures, for the renderer being drawn to */

static void load_sprites(void)
{
    for (int i = 0; i < NSPRITE; i++) {
        int n;
        sprite[i].px = stbi_load_from_memory(sprite[i].file, (int)(sprite[i].end - sprite[i].file), &sprite[i].w, &sprite[i].h, &n, 4);
        if (!sprite[i].px) { sprite[i].w = sprite[i].h = 1; }
    }
}
static void make_textures(SDL_Renderer *r, SDL_Texture **tex)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    for (int i = 0; i < NSPRITE; i++) {
        tex[i] = NULL;
        if (!sprite[i].px) continue;
        tex[i] = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, sprite[i].w, sprite[i].h);
        if (!tex[i]) continue;
        SDL_UpdateTexture(tex[i], NULL, sprite[i].px, sprite[i].w * 4);
        SDL_SetTextureBlendMode(tex[i], SDL_BLENDMODE_BLEND);
    }
}

/* ---- options ---- */
static int opt_window = 0;
static double opt_speed = 1.0;
static uint64_t opt_seed = 0;
static const char *opt_snapshot = NULL;
static double opt_time = 30;
static int opt_w = 1920, opt_h = 1200;

/* ---- colours ---- */
typedef struct { Uint8 r, g, b; } Rgb;
static const Rgb BG = {10, 9, 12}, INK = {30, 24, 20}, PAPER = {236, 226, 200}, GOLD = {205, 168, 92};

/* ---- geometry buffer ---- */
static SDL_Vertex *vb;
static int *ib;
static int nv, ni, vcap, icap;

static void vb_need(int v, int i)
{
    if (nv + v > vcap) { vcap = (nv + v) * 2; vb = realloc(vb, sizeof *vb * vcap); }
    if (ni + i > icap) { icap = (ni + i) * 2; ib = realloc(ib, sizeof *ib * icap); }
}
static inline SDL_Color col(Rgb c, double a)
{
    SDL_Color o = {c.r, c.g, c.b, (Uint8)(a < 0 ? 0 : a > 1 ? 255 : a * 255)};
    return o;
}
static inline void vert(double x, double y, SDL_Color c)
{
    vb[nv].position.x = (float)x; vb[nv].position.y = (float)y;
    vb[nv].color = c; vb[nv].tex_coord.x = vb[nv].tex_coord.y = 0;
    nv++;
}
static void quad(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3, SDL_Color c)
{
    vb_need(4, 6);
    int b = nv;
    vert(x0, y0, c); vert(x1, y1, c); vert(x2, y2, c); vert(x3, y3, c);
    ib[ni++] = b; ib[ni++] = b + 1; ib[ni++] = b + 2; ib[ni++] = b; ib[ni++] = b + 2; ib[ni++] = b + 3;
}
static void rect(double x, double y, double w, double h, SDL_Color c) { quad(x, y, x + w, y, x + w, y + h, x, y + h, c); }
static void disc(double cx, double cy, double r, SDL_Color centre, SDL_Color rim)
{
    int n = r < 4 ? 10 : r < 30 ? 28 : 56;
    vb_need(n + 1, 3 * n);
    int b = nv;
    vert(cx, cy, centre);
    for (int i = 0; i < n; i++) vert(cx + r * cos(i * 2 * M_PI / n), cy + r * sin(i * 2 * M_PI / n), rim);
    for (int i = 0; i < n; i++) { ib[ni++] = b; ib[ni++] = b + 1 + i; ib[ni++] = b + 1 + (i + 1) % n; }
}
/* a stroke with round ends */
static void line(double x0, double y0, double x1, double y1, double w, SDL_Color c)
{
    double dx = x1 - x0, dy = y1 - y0, l = hypot(dx, dy);
    if (l > 1e-9) {
        double nx = -dy / l * w / 2, ny = dx / l * w / 2;
        quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, c);
    }
    if (w > 2.5) { disc(x0, y0, w / 2, c, c); disc(x1, y1, w / 2, c, c); }
}
static void flush(SDL_Renderer *r)
{
    if (ni) SDL_RenderGeometry(r, NULL, vb, nv, ib, ni);
    nv = ni = 0;
}

static SDL_Renderer *ren; /* where the current frame is being drawn */

/* ---- lettering: pen strokes on a 4 x 6 grid, drawn with a round nib ---- */
static const char *GLYPH[] = {
    "A06 02 20 42 46|04 44", "B00 06 36 45 44 33 03|33 42 41 30 00", "C41 30 10 01 05 16 36 45",
    "D00 06 26 44 42 20 00", "E40 00 06 46|03 33", "F40 00 06|03 33", "G41 30 10 01 05 16 36 45 43 23",
    "H00 06|40 46|03 43", "I20 26|10 30|16 36", "J40 45 36 16 05", "K00 06|40 03 46", "L00 06 46",
    "M06 00 23 40 46", "N06 00 46 40", "O10 30 41 45 36 16 05 01 10", "P06 00 30 41 42 33 03",
    "Q10 30 41 45 36 16 05 01 10|24 46", "R06 00 30 41 42 33 03|23 46", "S41 30 10 01 02 13 33 44 45 36 16 05",
    "T00 40|20 26", "U00 05 16 36 45 40", "V00 26 40", "W00 16 23 36 40", "X00 46|40 06", "Y00 23 40|23 26",
    "Z00 40 06 46", "010 30 41 45 36 16 05 01 10", "111 20 26|16 36", "201 10 30 41 42 06 46",
    "301 10 30 41 42 33 23|33 44 45 36 16 05", "436 30 04 44", "540 00 03 33 44 45 36 16 05",
    "630 10 01 05 16 36 45 44 33 03", "700 40 16", "813 02 01 10 30 41 42 33 13 04 05 16 36 45 44 33",
    "943 13 02 01 10 30 41 45 36 16", ",25 26 17", ".26 26", ":22 22|25 25", "'20 21", "?01 10 30 41 42 24|26 26",
    "-13 33", "!20 24|26 26", "+03 43|21 25", "=02 42|04 44",
};
static const char *glyph(char ch)
{
    for (unsigned i = 0; i < sizeof GLYPH / sizeof *GLYPH; i++)
        if (GLYPH[i][0] == ch) return GLYPH[i] + 1;
    return NULL;
}
static double text_width(const char *str, double s) { return strlen(str) * 6.2 * s - 2.2 * s; }
/* text centred on cx, top at y; s is one grid step, so letters are 6s tall */
static void text(const char *str, double cx, double y, double s, double weight, SDL_Color c)
{
    double x = cx - text_width(str, s) / 2;
    for (; *str; str++, x += 6.2 * s) {
        const char *g = glyph(*str);
        if (!g) continue;
        int have = 0; double lx = 0, ly = 0;
        for (; *g; g++) {
            if (*g == '|') { have = 0; continue; }
            if (*g == ' ') continue;
            double gx = x + (g[0] - '0') * s, gy = y + (g[1] - '0') * s;
            g++;
            if (have) line(lx, ly, gx, gy, s * weight, c);
            disc(gx, gy, s * weight * (have && lx == gx && ly == gy ? 0.62 : 0.5), c, c); /* the nib is round */
            lx = gx; ly = gy; have = 1;
        }
    }
}

/* ---- randomness and timing helpers ---- */
static uint64_t seed_state;
static uint64_t next_rand(void)
{
    seed_state ^= seed_state << 13; seed_state ^= seed_state >> 7; seed_state ^= seed_state << 17;
    return seed_state;
}
static double frand(void) { return (next_rand() >> 11) * (1.0 / 9007199254740992.0); }
static double ramp(double t, double a, double b) { return t <= a ? 0 : t >= b ? 1 : (t - a) / (b - a); }
static double ease(double x) { return x * x * (3 - 2 * x); }


/* ---- the time ----
 *
 * The clock shows the time of day where the computer is: read at the start, and then
 * kept by the frame clock so that it moves smoothly. --clock HH:MM[:SS] starts it at
 * another time instead, to see the hour come round. */
static double opt_clock = -1;          /* seconds after midnight to start at, or -1: the real time */
static double tod0, anim_t;            /* the time of day at the start, and the seconds since */
static double T;                       /* the time of day now, in seconds after midnight */

static void read_time(void)
{
    if (opt_clock >= 0) { tod0 = opt_clock; return; }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    tod0 = tm ? tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec : 0;
}
static void update(double dt)
{
    anim_t += dt * opt_speed;
    T = fmod(tod0 + anim_t, 86400);
    /* the screensaver may run for hours: read the real clock again now and then,
     * so that it never drifts */
    static double since;
    since += dt;
    if (opt_clock < 0 && since > 60 && !opt_snapshot) { since = 0; read_time(); anim_t = 0; }
}
static void start_show(void)
{
    seed_state = opt_seed ? opt_seed * 0x9E3779B97F4A7C15ULL : ((uint64_t)time(NULL) << 20) ^ SDL_GetPerformanceCounter();
    if (!seed_state) seed_state = 1;
    for (int i = 0; i < 6; i++) next_rand();
    anim_t = 0;
    read_time();
    T = tod0;
}
/* a number from 0 to 1 that is the same for the same n, and looks random */
static double hash01(uint64_t n)
{
    n = (n + 0x9E3779B97F4A7C15ULL) * 0xBF58476D1CE4E5B9ULL;
    n ^= n >> 31; n *= 0x94D049BB133111EBULL; n ^= n >> 29;
    return (n >> 11) * (1.0 / 9007199254740992.0);
}

/* ---- what he says: his own lines, every quarter of an hour, with the film each is
 * from (as Wikiquote gives them) ---- */
typedef struct { const char *say, *film; } Saying;
#define AC "ANIMAL CRACKERS, 1930"
#define CN "THE COCOANUTS, 1929"
#define DS "DUCK SOUP, 1933"
#define HF "HORSE FEATHERS, 1932"
#define MB "MONKEY BUSINESS, 1931"
#define NO "A NIGHT AT THE OPERA, 1935"
#define DR "A DAY AT THE RACES, 1937"
static const Saying SAYINGS[] = {
    {"ONE MORNING I SHOT AN ELEPHANT IN MY PAJAMAS. HOW HE GOT IN MY PAJAMAS, I DON'T KNOW.", AC},
    {"WHY, YOU'RE ONE OF THE MOST BEAUTIFUL WOMEN I'VE EVER SEEN, AND THAT'S NOT SAYING MUCH FOR YOU.", AC},
    {"AFRICA IS GOD'S COUNTRY, AND HE CAN HAVE IT.", AC},
    {"HELLO, I MUST BE GOING.", AC},
    {"THE PRINCIPAL ANIMALS INHABITING THE AFRICAN JUNGLE ARE MOOSE, ELKS AND KNIGHTS OF PYTHIAS.", AC},
    {"YOU TAKE CARE OF YOUR ANIMALS AND I'LL TAKE CARE OF MINE!", AC},
    {"WHEN YOU GET NEAR A SONG, PLAY IT.", AC},
    {"HOW MUCH WOULD YOU WANT TO RUN INTO AN OPEN MANHOLE?", AC},
    {"PARDON ME WHILE I HAVE A STRANGE INTERLUDE.", AC},
    {"THREE YEARS AGO I CAME TO FLORIDA WITHOUT A NICKEL IN MY POCKET. NOW I'VE GOT A NICKEL IN MY POCKET.", CN},
    {"NOW THIS IS OUR CEMETERY. I'VE GOT A LIST OF A HUNDRED PEOPLE JUST DYING TO GET IN.", CN},
    {"I'M GOING TO GET YOU A STEADY POSITION. AND IF I CAN ARRANGE IT, IT'LL BE HORIZONTAL.", CN},
    {"ICE WATER IN ROOM 318? NO KIDDING, WHERE'D YOU GET IT?", CN},
    {"DON'T PICK THEIR POCKETS, JUST MINGLE WITH THEM.", CN},
    {"OH, I SEE! YOU'RE JUST AN IDLE ROOMER!", CN},
    {"WELL, I DON'T KNOW WHY A NO CHICKEN. I'M A STRANGER HERE MYSELF.", CN},
    {"A YES LIKE THAT WAS ONCE RESPONSIBLE FOR ME JUMPING OUT OF A WINDOW.", CN},
    {"I COULD DANCE WITH YOU TILL THE COWS COME HOME. ON SECOND THOUGHT, I'D RATHER DANCE WITH THE COWS TILL YOU CAME HOME.", DS},
    {"DON'T LOOK NOW, BUT THERE'S ONE MAN TOO MANY IN THIS ROOM, AND I THINK IT'S YOU.", DS},
    {"I'LL SEE MY LAWYER ABOUT THIS AS SOON AS HE GRADUATES FROM LAW SCHOOL.", DS},
    {"CHICOLINI HERE MAY TALK LIKE AN IDIOT, AND LOOK LIKE AN IDIOT. BUT DON'T LET THAT FOOL YOU. HE REALLY IS AN IDIOT.", DS},
    {"REMEMBER, YOU'RE FIGHTING FOR THIS WOMAN'S HONOR, WHICH IS PROBABLY MORE THAN SHE EVER DID.", DS},
    {"YOU MUST HAVE BEEN VACCINATED WITH A PHONOGRAPH NEEDLE.", DS},
    {"GET ME HEADQUARTERS. NOT HINDQUARTERS, HEADQUARTERS!", DS},
    {"LET ME OUT OF HERE, OR THROW ME A MAGAZINE!", DS},
    {"YOU'VE GOT THE BRAIN OF A FOUR-YEAR-OLD BOY, AND I BET HE WAS GLAD TO GET RID OF IT.", HF},
    {"WELL, I THOUGHT MY RAZOR WAS DULL UNTIL I HEARD THIS SPEECH.", HF},
    {"I'D HORSEWHIP YOU, IF I HAD A HORSE.", HF},
    {"I MARRIED YOUR MOTHER BECAUSE I WANTED CHILDREN. IMAGINE MY DISAPPOINTMENT WHEN YOU ARRIVED.", HF},
    {"OH, I LOVE SITTING ON YOUR LAP. I COULD SIT HERE ALL DAY IF YOU DIDN'T STAND UP.", HF},
    {"WHY DON'T YOU BORE A HOLE IN YOURSELF AND LET THE SAP RUN OUT?", HF},
    {"IS THIS STUFF ON THE LEVEL OR ARE YOU JUST MAKING IT UP AS YOU GO ALONG?", HF},
    {"I'M ALWAYS THAT WAY AFTER I EAT RADISHES.", HF},
    {"SIR, ARE YOU TRYING TO OFFER ME A BRIBE? HOW MUCH?", MB},
    {"WITH A LITTLE STUDY YOU'LL GO A LONG WAY. AND I WISH YOU'D START NOW.", MB},
    {"MADAM, I RESENT THAT. SOME OF MY BEST FRIENDS ARE HOUSEWIVES!", MB},
    {"ALL RIGHT, GIVE ME THE DOLLAR, I'LL KEEP THE CHANGE.", MB},
    {"AFRAID? ME? A MAN WHO'S LICKED HIS WEIGHT IN WILD CATERPILLARS? AFRAID? YOU BET I'M AFRAID!", MB},
    {"LADIES AND GENTLEMEN. I GUESS THAT TAKES IN MOST OF YOU.", NO},
    {"EVERYTHING ABOUT YOU REMINDS ME OF YOU. EXCEPT YOU.", NO},
    {"YES. JUST ME AND MY MEMORIES. I'M PRACTICALLY A HERMIT.", NO},
    {"THEY'RE FAMOUS AS AVIATORS, BUT YOU NOTICE THEY'RE TRAVELING BY BOAT.", NO},
    {"CAN YOU SLEEP ON YOUR STOMACH WITH SUCH BIG BUTTONS ON YOUR PAJAMAS?", NO},
    {"WELL, I HAVEN'T SAID ANYTHING WORTH HEARING.", NO},
    {"EITHER HE'S DEAD OR MY WATCH HAS STOPPED!", DR},
    {"TAKE ONE OF THOSE EVERY HALF MILE AND CALL ME IF THERE'S ANY CHANGE.", DR},
    {"DON'T DRINK THAT POISON! THAT'S FOUR DOLLARS AN OUNCE.", DR},
    {"IF I HOLD YOU ANY CLOSER, I'LL BE IN BACK OF YA!", DR},
    {"THE LAST PATIENT I GAVE ONE OF THOSE TO WON THE KENTUCKY DERBY.", DR},
    {"ALL I WANTED WAS A HORSE, NOT A PUBLIC LIBRARY.", DR},
    {"IT'S THE OLD, OLD STORY. BOY MEETS GIRL - ROMEO AND JULIET - MINNEAPOLIS AND ST. PAUL!", DR},
};
#define NSAYING (int)(sizeof SAYINGS / sizeof *SAYINGS)
#define QUARTER 900.0
#define CARD_LEN 10.0                  /* how long a saying stays up */
/* the saying for a quarter of an hour: a different order each day */
static const Saying *saying_for(double t)
{
    uint64_t day = (uint64_t)(time(NULL) / 86400), q = (uint64_t)(t / QUARTER);
    return &SAYINGS[(int)(hash01(day * 131 + q) * NSAYING) % NSAYING];
}
#define HOUR_SHOW 12.0                 /* how long the hour's business takes */

/* ---- Gummo ----
 * In about one minute in four he peeks in from the left for a few seconds: in, a
 * look, a raised hat, a wink or a finger to his lips, and gone. */
#define GUMMO_LEN 4.6
static int gummo_at(double t, double *q, int *frame)
{
    double m = floor(t / 60), s = t - m * 60;
    if (hash01((uint64_t)m * 7 + 3) > 0.25) return 0;
    double s0 = 18 + hash01((uint64_t)m * 7 + 5) * 30;
    if (s < s0 || s > s0 + GUMMO_LEN) return 0;
    double u = s - s0;
    *q = ease(ramp(u, 0, 0.45)) * (1 - ease(ramp(u, GUMMO_LEN - 0.45, GUMMO_LEN)));
    *frame = u < 1.5 ? 0 : 1 + (int)(hash01((uint64_t)m * 7 + 6) * 3);
    return 1;
}


/* ---- drawing ---- */
static double unit;
static int sw, sh;
static double rx0, ry0, rw, rh;       /* where the current scene's picture is on the screen */

/* a picture covering the screen, cut to fit, and pushed in a little over `push` */
static void backdrop(int which, double push)
{
    double s = fmax((double)sw / sprite[which].w, (double)sh / sprite[which].h) * (1 + 0.04 * push);
    rw = sprite[which].w * s; rh = sprite[which].h * s;
    rx0 = (sw - rw) / 2; ry0 = (sh - rh) / 2 - rh * 0.012 * push;
    flush(ren);
    if (!cur_tex || !cur_tex[which]) return;
    SDL_FRect d = {(float)rx0, (float)ry0, (float)rw, (float)rh};
    SDL_RenderCopyF(ren, cur_tex[which], NULL, &d);
}
static void old_film(void)
{
    double f = 0.03 + 0.02 * sin(anim_t * 37) * sin(anim_t * 13);
    rect(0, 0, sw, sh, col(BG, f));
    uint64_t keep = seed_state;
    seed_state = 1 + (uint64_t)(anim_t * 24);
    for (int i = 0; i < 3; i++) {                  /* a scratch or two */
        if (frand() < 0.5) continue;
        double x = frand() * sw;
        line(x, 0, x + (frand() - 0.5) * unit, sh, unit * 0.08, col(PAPER, 0.12));
    }
    for (int i = 0; i < 40; i++) {                 /* dust */
        double x = frand() * sw, y = frand() * sh, r = unit * (0.05 + frand() * 0.12);
        disc(x, y, r, col(frand() < 0.5 ? INK : PAPER, 0.25), col(INK, 0));
    }
    seed_state = keep;
    flush(ren);
}

/* ---- Groucho ----
 *
 * Everything on his face is placed in the face picture's own pixels (as Gemini drew
 * it, 956 x 1380), so that the parts go where they belong at any size. */
#define EYE_LX 352.0
#define EYE_RX 602.0
#define EYE_Y 446.0
#define DIAL_R 84.0                    /* the clock face inside each lens, */
#define BIG 1.32                       /* and the glasses made this much bigger, to read them */
#define GLASS_LX 118.0                 /* the lenses' middles in the glasses picture */
#define GLASS_RX 356.0
#define GLASS_Y 92.0
#define BROW_Y 285.0                   /* where the eyebrows sit, above the glasses */
#define BROW_SCALE 0.72
#define STACHE_X 478.0                 /* the middle of his moustache, on the face, */
#define STACHE_Y 651.0
#define GRIN_STACHE_X 230.0            /* and on the grin */
#define GRIN_STACHE_Y 52.0
#define CIGAR_X 585.0                  /* the corner of his mouth, where the cigar goes */
#define CIGAR_Y 712.0
#define CIGAR_SCALE 0.62
#define CIGAR_TILT -14.0

static double fs, fx0, fy0;            /* the face's scale and where its top left is */
static void at_face(double px, double py, double *x, double *y) { *x = fx0 + px * fs; *y = fy0 + py * fs; }

/* a picture drawn with the point (ax, ay) of it at (x, y), scaled and turned about that point */
static void put(int which, double x, double y, double ax, double ay, double scale, double angle, double alpha, int flip)
{
    flush(ren);
    if (which < 0 || !cur_tex || !cur_tex[which] || alpha <= 0.01) return;
    SDL_FRect d = {(float)(x - ax * scale), (float)(y - ay * scale), (float)(sprite[which].w * scale), (float)(sprite[which].h * scale)};
    SDL_FPoint c = {(float)(ax * scale), (float)(ay * scale)};
    SDL_SetTextureAlphaMod(cur_tex[which], (Uint8)(fmin(alpha, 1) * 255));
    SDL_RenderCopyExF(ren, cur_tex[which], NULL, &d, angle, &c, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(cur_tex[which], 255);
}

/* One lens of his glasses as a clock face: paper white, ticks round the rim and the
 * four quarters numbered (the hours 12, 3, 6, 9, or the minutes 60, 15, 30, 45, with
 * a small tick for every minute), and his pupil as the hand, pointing at `turn` (0 to
 * 1, clockwise from the top). */
static void dial(double ex, double ey, double turn, const char *label, int minutes)
{
    double cx, cy, r = DIAL_R * BIG * fs;
    at_face(ex, ey, &cx, &cy);
    disc(cx, cy, r, col((Rgb){253, 251, 242}, 1), col((Rgb){236, 230, 214}, 1));
    for (int i = 0; i < (minutes ? 60 : 12); i++) {          /* ticks round the rim */
        int step = minutes ? i % 5 == 0 : 1, quarter = minutes ? i % 15 == 0 : i % 3 == 0;
        double a = i * 2 * M_PI / (minutes ? 60 : 12);
        double r0 = r * (quarter ? 0.84 : step ? 0.88 : 0.93), r1 = r * 0.97;
        line(cx + sin(a) * r0, cy - cos(a) * r0, cx + sin(a) * r1, cy - cos(a) * r1,
             r * (quarter ? 0.05 : step ? 0.035 : 0.016), col(INK, 1));
    }
    static const char *QH[] = {"12", "3", "6", "9"}, *QM[] = {"60", "15", "30", "45"};
    const char **Q = minutes ? QM : QH;
    for (int i = 0; i < 4; i++) {                            /* the quarters, big and bold */
        double a = i * M_PI / 2, sz = r * 0.047, rr = i % 2 ? r * 0.80 - text_width(Q[i], sz) / 2 : r * 0.60;
        text(Q[i], cx + sin(a) * rr, cy - cos(a) * rr - 3 * sz, sz, 1.25, col(INK, 1));
    }
    (void)label;
    flush(ren);
    /* the hand: a tapering black hand from the middle, with his pupil at its tip */
    double a = turn * 2 * M_PI, pr = r * 0.50, px = cx + sin(a) * pr, py = cy - cos(a) * pr;
    double nx = cos(a), ny = sin(a), w = r * 0.045;
    vb_need(3, 3);
    int k = nv;
    vert(cx + nx * w, cy + ny * w, col(INK, 1)); vert(cx - nx * w, cy - ny * w, col(INK, 1)); vert(px, py, col(INK, 1));
    ib[ni++] = k; ib[ni++] = k + 1; ib[ni++] = k + 2;
    disc(cx, cy, r * 0.07, col(INK, 1), col(INK, 1));
    disc(cx, cy, r * 0.025, col(GOLD, 1), col(GOLD, 1));
    put(SP_pupil, px, py, sprite[SP_pupil].w / 2.0, sprite[SP_pupil].h / 2.0, fs * 0.62, 0, 1, 0);
}

/* the cigar for this part of the hour, where its lit end is, and where it is held */
static int cigar_now(double hp, double *tipx, double *tipy, double *ay)
{
    int c = hp < 4.0 ? (hp < 1.6 ? SP_cigar3 : SP_cigar0) : hp < 1200 ? SP_cigar1 : hp < 2400 ? SP_cigar2 : SP_cigar3;
    *ay = c == SP_cigar3 ? 46 : sprite[c].h / 2.0;
    *tipx = c == SP_cigar3 ? 150 : sprite[c].w - 12;
    *tipy = c == SP_cigar3 ? 44 : sprite[c].h / 2.0;
    return c;
}

static void smoke(double x, double y, double strength)
{
    for (int i = 0; i < 4; i++) {
        double life = 4.5, age = fmod(anim_t + i * life / 4, life), q = age / life;
        double sx = x + sin(anim_t * 0.9 + i * 2.1) * fs * 30 * q + fs * 260 * q, sy = y - fs * 300 * q;
        sx += fs * 120 * q;
        double a = strength * 0.75 * sin(M_PI * q);
        put(SP_smoke, sx, sy, sprite[SP_smoke].w / 2.0, sprite[SP_smoke].h, fs * (0.5 + 0.7 * q), sin(anim_t * 0.7 + i) * 12, a, i & 1);
    }
}

/* the saying, on a card held up from below on a stick, in silver on black as in
 * the films' title cards */
static void saying_card(const Saying *sy, double up)
{
    const int WRAP = 24;
    char lines[6][64]; int n = 0, len = 0, most = 0;
    const char *p = sy->say; lines[0][0] = 0;
    while (*p && n < 6) {                                  /* wrap at WRAP letters */
        const char *e = p; while (*e && *e != ' ') e++;
        int wl = (int)(e - p);
        if (len && len + 1 + wl > WRAP) { n++; len = 0; if (n >= 6) break; lines[n][0] = 0; }
        if (len) lines[n][len++] = ' ';
        memcpy(lines[n] + len, p, wl); len += wl; lines[n][len] = 0;
        if (len > most) most = len;
        p = *e ? e + 1 : e;
    }
    n++;
    double w = sw * 0.5, cx = sw / 2.0;
    double s = (w - unit * 8) / (most * 6.2);              /* as big as fits across the card */
    double lh = s * 10, fsz = s * 0.55, h = lh * n + s * 8 + fsz * 9;
    double y = sh + unit * 2 - (sh * 0.05 + h + unit * 2) * up;
    SDL_Color gold = col(GOLD, 1), silver = col((Rgb){232, 230, 222}, 1);
    line(cx, y + h, cx, sh + unit, unit * 1.1, col(INK, 1));            /* the stick */
    line(cx, y + h, cx, sh + unit, unit * 0.7, col((Rgb){150, 108, 62}, 1));
    rect(cx - w / 2 + unit * 0.6, y + unit * 0.8, w, h, col(INK, 0.5));
    rect(cx - w / 2, y, w, h, col((Rgb){20, 19, 22}, 1));
    line(cx - w / 2 + unit, y + unit, cx + w / 2 - unit, y + unit, unit * 0.5, gold);
    line(cx + w / 2 - unit, y + unit, cx + w / 2 - unit, y + h - unit, unit * 0.5, gold);
    line(cx + w / 2 - unit, y + h - unit, cx - w / 2 + unit, y + h - unit, unit * 0.5, gold);
    line(cx - w / 2 + unit, y + h - unit, cx - w / 2 + unit, y + unit, unit * 0.5, gold);
    for (int i = 0; i < n; i++) text(lines[i], cx, y + s * 5 + i * lh, s, 1.0, silver);
    text(sy->film, cx, y + s * 5 + n * lh + fsz * 1.5, fsz, 1.0, col(GOLD, 0.9));
    flush(ren);
}

static void draw_scene(SDL_Renderer *r, int w, int h)
{
    ren = r; sw = w; sh = h;
    unit = (w < h ? w : h) * 0.0125;
    SDL_SetRenderDrawColor(r, BG.r, BG.g, BG.b, 255);
    SDL_RenderClear(r);
    backdrop(SP_backdrop, 0);
    rect(0, 0, sw, sh, col(BG, 0.25));
    disc(sw / 2.0, sh * 0.55, sh * 0.62, col((Rgb){255, 214, 150}, 0.16), col((Rgb){255, 214, 150}, 0));  /* the spotlight */
    flush(ren);

    double hp = fmod(T, 3600), mp = fmod(T, 60);
    double minutes = hp / 60;
    double gq; int gframe;
    int gummo = gummo_at(T, &gq, &gframe);

    /* his head, nearly the height of the screen, breathing */
    fs = sh * 0.96 / sprite[SP_face].h;
    fx0 = sw / 2.0 - sprite[SP_face].w * fs / 2;
    fy0 = sh - sprite[SP_face].h * fs + sin(anim_t * 1.4) * sh * 0.004;
    put(SP_face, fx0, fy0, 0, 0, fs, 0, 1, 0);

    /* the clocks in his glasses: hours on the left, minutes on the right */
    /* the glasses, made big and set between his eyes; the lenses' middles are the dials' */
    double h12 = fmod(T / 3600, 12), gk = (EYE_RX - EYE_LX) / (GLASS_RX - GLASS_LX) * BIG;
    double mid = (EYE_LX + EYE_RX) / 2, half = (GLASS_RX - GLASS_LX) / 2 * gk;
    dial(mid - half, EYE_Y, h12 / 12, "HOURS", 0);
    dial(mid + half, EYE_Y, minutes / 60, "MINUTES", 1);
    double gx, gy;
    at_face(mid, EYE_Y, &gx, &gy);
    put(SP_glasses, gx, gy, (GLASS_LX + GLASS_RX) / 2, GLASS_Y, fs * gk, 0, 1, 0);

    /* the eyebrows: a waggle as each minute begins, a long one on the hour, and up
     * in surprise when Gummo looks in */
    double wag = mp < 1.6 ? sin(mp * 2 * M_PI * 2.5) * (1 - mp / 1.6) : 0;
    if (hp < HOUR_SHOW) wag = sin(hp * 2 * M_PI * 2.5) * (hp < 9 ? 1 : (HOUR_SHOW - hp) / 3);
    double lift = fabs(wag) * 46 + (gummo ? 34 * gq : 0);
    for (int side = 0; side < 2; side++) {
        double bx, by;
        at_face(side ? EYE_RX + 12 : EYE_LX - 12, BROW_Y - lift, &bx, &by);
        int b = side ? SP_brow_r : SP_brow_l;
        put(b, bx, by, sprite[b].w / 2.0, sprite[b].h / 2.0, fs * BROW_SCALE, (side ? 1 : -1) * wag * 9, 1, 0);
    }

    /* the grin, on the hour and when he catches Gummo */
    double grin = hp < HOUR_SHOW ? ramp(hp, 3.6, 4.0) * (1 - ramp(hp, HOUR_SHOW - 1, HOUR_SHOW)) : 0;
    if (gummo && gframe) grin = fmax(grin, gq);
    if (grin > 0) {
        double mx, my;
        at_face(STACHE_X, STACHE_Y, &mx, &my);
        put(SP_grin, mx, my, GRIN_STACHE_X, GRIN_STACHE_Y, fs, 0, grin, 0);
        /* and his own moustache back over it: the grin was drawn with a little one */
        double sx, sy;
        at_face(STACHE_ON_FACE_X, STACHE_ON_FACE_Y - 6 * grin, &sx, &sy);
        put(SP_stache, sx, sy, 0, 0, fs, 0, 1, 0);
    }

    /* the cigar, burning down through the hour, and ticking off the seconds: it
     * snaps up a notch on each second and down on the next, as he chews it. On the
     * hour he taps it, the long ash breaks off and falls, and he lights a new one. */
    double tx, ty, ay, cxp, cyp;
    int c = cigar_now(hp, &tx, &ty, &ay);
    double sec = fmod(T, 1.0), snap = 1 - exp(-sec * 40);
    int odd = (long)floor(T) & 1;
    double tick = (odd ? 1 : -1) * (2 * snap - 1) * 2.4;
    double tap = hp < 0.6 ? sin(hp / 0.6 * M_PI * 3) * 7 * (1 - hp / 0.6) : 0;
    double tilt = CIGAR_TILT + tick + tap;
    at_face(CIGAR_X, CIGAR_Y, &cxp, &cyp);
    double k = fs * CIGAR_SCALE, ta = tilt * M_PI / 180;
    if (hp < 1.6) {                                        /* the stub, and the ash falling */
        put(SP_stub, cxp, cyp, 0, 46, k, tilt, 1, 0);
        double dx = ASH_ON_CIGAR_X * k, dy = (ASH_ON_CIGAR_Y - 46) * k;
        double axp = cxp + dx * cos(ta) - dy * sin(ta), ayp = cyp + dx * sin(ta) + dy * cos(ta);
        double f = fmax(0, hp - 0.35);                     /* it holds for the tap, then goes */
        double fall = 0.5 * 0.9 * sh * f * f;
        double alpha = 1 - ramp(ayp + fall, sh * 0.85, sh * 1.05);
        put(SP_ash, axp + f * fs * 60, ayp + fall, 0, 0, k, tilt + f * 70, alpha, 0);
        /* a puff of crumbs where it hits his lapel */
        if (f > 0) for (int i = 0; i < 8; i++) {
            double q = ramp(hp, 0.35 + i * 0.05, 1.6);
            if (q <= 0 || q >= 1) continue;
            double bx = axp + fs * (40 + 30 * sin(i * 2.3)) + q * fs * 50 * cos(i), by = ayp + fall * 0.9 + q * fs * 30 * sin(i * 1.7);
            disc(bx, by, fs * 5 * (1 - q), col((Rgb){190, 186, 178}, 0.8 * (1 - q)), col((Rgb){190, 186, 178}, 0));
        }
        flush(ren);
    } else {
        put(c, cxp, cyp, 0, ay, k, tilt, 1, 0);
    }
    if (c != SP_cigar0 && hp >= 1.6) {
        double dx = tx * k, dy = (ty - ay) * k;
        double sx = cxp + dx * cos(ta) - dy * sin(ta), sy = cyp + dx * sin(ta) + dy * cos(ta);
        double puff = hp < 6 ? 1 + 2 * (1 - ramp(hp, 4, 6)) : 1;
        smoke(sx, sy, fmin(puff, 1.6));
    }

    /* a saying every quarter of an hour; on the hour, after the new cigar */
    double qp = fmod(T, QUARTER), q0 = hp < QUARTER ? 3.8 : 0.3;
    if (qp < q0 + CARD_LEN) {
        double up = ease(ramp(qp, q0, q0 + 0.8)) * (1 - ease(ramp(qp, q0 + CARD_LEN - 0.8, q0 + CARD_LEN)));
        if (up > 0) saying_card(saying_for(T), up);
    }

    /* Gummo, at the left edge */
    if (gummo) {
        int g = SP_gummo0 + gframe;
        double gs = sh * 0.52 / sprite[SP_gummo0].h;
        put(g, -sprite[g].w * gs * (1 - gq), sh - sprite[g].h * gs, 0, 0, gs, 0, 1, 0);
    }
    old_film();
}


typedef struct { SDL_Window *win; SDL_Renderer *ren; SDL_Texture *scene, *tex[NSPRITE]; int w, h; } View;

static void size_view(View *v)
{
    int ow, oh;
    SDL_RendererInfo info;
    SDL_GetRendererOutputSize(v->ren, &ow, &oh);
    if (ow < 1 || oh < 1) return;
    int ss = 2; /* drawn at twice the size and scaled down, for smooth edges */
    if (SDL_GetRendererInfo(v->ren, &info) == 0) {
        if (!(info.flags & SDL_RENDERER_TARGETTEXTURE) || (info.flags & SDL_RENDERER_SOFTWARE)) ss = 0;
        else if (info.max_texture_width && (ow * 2 > info.max_texture_width || oh * 2 > info.max_texture_height)) ss = 0;
    }
    if (v->w == ow * (ss ? ss : 1) && v->h == oh * (ss ? ss : 1) && (v->scene || !ss)) return;
    if (v->scene) SDL_DestroyTexture(v->scene);
    v->scene = NULL; v->w = ow; v->h = oh;
    if (!ss) return;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    v->scene = SDL_CreateTexture(v->ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, ow * ss, oh * ss);
    if (v->scene) { v->w = ow * ss; v->h = oh * ss; }
}
static void render(View *v)
{
    if (v->scene) SDL_SetRenderTarget(v->ren, v->scene);
    cur_tex = v->tex;
    draw_scene(v->ren, v->w, v->h);
    if (v->scene) { SDL_SetRenderTarget(v->ren, NULL); SDL_RenderCopy(v->ren, v->scene, NULL, NULL); }
    SDL_RenderPresent(v->ren);
}
static int make_view(View *v, int display)
{
    Uint32 flags = SDL_WINDOW_ALLOW_HIGHDPI;
    int x = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
    if (opt_window) flags |= SDL_WINDOW_RESIZABLE;
    else flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
    memset(v, 0, sizeof *v);
    v->win = SDL_CreateWindow("The Groucho Clock", x, x, 1200, 750, flags);
    if (!v->win) return 0;
    v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!v->ren) v->ren = SDL_CreateRenderer(v->win, -1, SDL_RENDERER_SOFTWARE);
    if (!v->ren) return 0;
    SDL_SetRenderDrawBlendMode(v->ren, SDL_BLENDMODE_BLEND);
    make_textures(v->ren, v->tex);
    return 1;
}


static int snapshot(void)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, opt_w, opt_h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!r) { fprintf(stderr, "groucho-clock: %s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_Texture *tex[NSPRITE];
    make_textures(r, tex);
    cur_tex = tex;
    start_show();
    for (double t = 0; t < opt_time; t += 1.0 / 60) update(1.0 / 60);
    draw_scene(r, opt_w, opt_h);
    SDL_RenderPresent(r);
    if (SDL_SaveBMP(surf, opt_snapshot) != 0) { fprintf(stderr, "groucho-clock: %s\n", SDL_GetError()); return 1; }
    return 0;
}

static void usage(void)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Groucho Clock",
        "The Groucho Clock screensaver\n\n"
        "  --window        run in a window (Esc or Q quits)\n"
        "  --clock HH:MM   start at that time of day instead of the real time\n"
        "  --speed X       how fast time goes, 1 = normal\n"
        "  --seed N        repeatable sequence\n"
        "  --snapshot FILE.bmp [--time SECONDS] [--size WxH]\n"
        "                  write a picture of the screensaver after SECONDS and exit\n", NULL);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 's' || a[1] == 'S') && !a[2]) continue;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'p' || a[1] == 'P') && (!a[2] || a[2] == ':')) return 0;
        else if ((a[0] == '/' || a[0] == '-') && (a[1] == 'c' || a[1] == 'C') && (!a[2] || a[2] == ':')) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Groucho Clock", "The Groucho Clock screensaver has no settings.", NULL);
            return 0;
        }
        else if (!strcmp(a, "--window")) opt_window = 1;
        else if (!strcmp(a, "--speed") && i + 1 < argc) opt_speed = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--seed") && i + 1 < argc) opt_seed = SDL_strtoull(argv[++i], NULL, 10);
        else if (!strcmp(a, "--snapshot") && i + 1 < argc) opt_snapshot = argv[++i];
        else if (!strcmp(a, "--clock") && i + 1 < argc) {
            int hh = 0, mm = 0, ss = 0;
            if (sscanf(argv[++i], "%d:%d:%d", &hh, &mm, &ss) < 2) { usage(); return 1; }
            opt_clock = hh * 3600 + mm * 60 + ss;
        }
        else if (!strcmp(a, "--time") && i + 1 < argc) opt_time = SDL_atof(argv[++i]);
        else if (!strcmp(a, "--size") && i + 1 < argc) {
            const char *x = SDL_strchr(argv[++i], 'x');
            opt_w = SDL_atoi(argv[i]); opt_h = x ? SDL_atoi(x + 1) : 0;
            if (opt_w < 64 || opt_h < 64) { usage(); return 1; }
        }
        else { usage(); return 1; }
    }
    if (opt_speed < 0.05) opt_speed = 0.05;
    load_sprites();

    if (opt_snapshot) return snapshot();
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "groucho-clock: %s\n", SDL_GetError()); return 1; }

    View views[8];
    int nviews = 0, ndisp = opt_window ? 1 : SDL_GetNumVideoDisplays();
    if (ndisp < 1) ndisp = 1;
    for (int d = 0; d < ndisp && nviews < 8; d++)
        if (make_view(&views[nviews], d)) nviews++;
    if (!nviews) { fprintf(stderr, "groucho-clock: %s\n", SDL_GetError()); return 1; }
    if (!opt_window) SDL_ShowCursor(SDL_DISABLE);
    start_show();

    Uint64 freqc = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter(), start = last;
    double moved = 0;
    int running = 1;
    while (running) {
        SDL_Event e;
        double since = (double)(SDL_GetPerformanceCounter() - start) / freqc;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) running = 0;
            else if (e.type == SDL_KEYDOWN) {
                if (!opt_window || e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q) running = 0;
            } else if (!opt_window && since > 1.0) {
                if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERDOWN) running = 0;
                else if (e.type == SDL_MOUSEMOTION) {
                    moved += abs(e.motion.xrel) + abs(e.motion.yrel);
                    if (moved > 24) running = 0;
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last) / freqc;
        last = now;
        if (dt > 0.1) dt = 0.1;
        update(dt);
        for (int i = 0; i < nviews; i++) { size_view(&views[i]); render(&views[i]); }
        SDL_Delay(views[0].scene ? 1 : 12);
    }
    SDL_Quit();
    return 0;
}

