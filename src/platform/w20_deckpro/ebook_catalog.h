/** eBook story catalog */
#pragma once

#include "ebook_emperor.h"
#include "ebook_princess_pea.h"
#include "ebook_fir_tree.h"
#include "ebook_leap_frog.h"
#include "ebook_bell.h"

typedef struct {
    const char *title;
    const char *const *pages;
    int page_count;
} ebook_story_t;

static const ebook_story_t ebook_catalog[] = {
    { "THE EMPEROR'S NEW CLOTHES", ebook_emperor_pages, ebook_emperor_page_count },
    { "THE PRINCESS AND THE PEA", ebook_princess_pea_pages, ebook_princess_pea_page_count },
    { "THE FIR TREE", ebook_fir_tree_pages, ebook_fir_tree_page_count },
    { "THE LEAP-FROG", ebook_leap_frog_pages, ebook_leap_frog_page_count },
    { "THE BELL", ebook_bell_pages, ebook_bell_page_count },
};

static const int ebook_catalog_count =
    (int)(sizeof(ebook_catalog) / sizeof(ebook_catalog[0]));

