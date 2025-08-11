/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_DEJAVIEW_PUBLIC_TE_CATEGORY_MACROS_H_
#define INCLUDE_DEJAVIEW_PUBLIC_TE_CATEGORY_MACROS_H_

#include "dejaview/public/track_event.h"

/* This header provides helper macros to list and register dejaview track event
   categories.

   Example:

   ```
   #define MY_CATEGORIES(C)                                   \
     C(c1, "c1", "My category 1 description", "tag1", "tag2") \
     C(c2, "c2", "My category 2 description", "tag1")         \
     C(c3, "c3", "My category 3 description")

   DEJAVIEW_TE_CATEGORIES_DEFINE(MY_CATEGORIES)

   //...

   int main() {
     //..
    DEJAVIEW_TE_REGISTER_CATEGORIES(MY_CATEGORIES);
   ```

   Three categories are defined (as global variables) `c1`, `c2` and `c3`. The
   tracing service knows them as "c1", "c2" and "c3" respectively. The extra
   strings in `C()` after the description are the tags (there can be zero to
   four tags).
*/

// Implementation details:
//
// The macros have been tested on GCC, clang and MSVC. The MSVC traditional
// preprocessor behaves slightly differently (see
// https://learn.microsoft.com/en-us/cpp/preprocessor/preprocessor-experimental-overview)
// so it sometimes requires an extra level of indirection with an intermediate
// macro.
//
// The header is not technically C99 or C++11 standard compliant for a single
// reason: sometimes `...` receives zero arguments (this has been relaxed in the
// post 2020 standards). Despite this, the header never uses __VA_ARGS__ when
// it would expand to zero arguments, because the behavior is compiler
// dependent.

#if defined(__GNUC__)
#pragma GCC system_header
#endif

#define DEJAVIEW_I_TE_CAT_DESCRIPTION_GET(D, ...) D
#define DEJAVIEW_I_TE_CAT_DESCRIPTION_CALL(MACRO, ARGS) MACRO ARGS
#define DEJAVIEW_I_TE_CAT_DESCRIPTION(D, ...) \
  DEJAVIEW_I_TE_CAT_DESCRIPTION_CALL(DEJAVIEW_I_TE_CAT_DESCRIPTION_GET, (D))

#define DEJAVIEW_I_TE_CAT_NUM_TAGS_(A, B, C, D, E, N, ...) N
#define DEJAVIEW_I_TE_CAT_NUM_TAGS(...) \
  DEJAVIEW_I_TE_CAT_NUM_TAGS_(__VA_ARGS__, 4, 3, 2, 1, 0)

#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_0(...)
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_1(NAME, DESC, TAG1) \
  static const char* NAME##_tags[] = {TAG1};
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_2(NAME, DESC, TAG1, TAG2) \
  static const char* NAME##_tags[] = {TAG1, TAG2};
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_3(NAME, DESC, TAG1, TAG2, TAG3) \
  static const char* NAME##_tags[] = {TAG1, TAG2, TAG3};
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_4(NAME, DESC, TAG1, TAG2, TAG3, TAG4) \
  static const char* NAME##_tags[] = {TAG1, TAG2, TAG3, TAG4};
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_GET_MACRO(NAME, _1, _2, _3, _4, MACRO, \
                                                ...)                         \
  MACRO
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS_CALL(MACRO, ARGS) MACRO ARGS
#define DEJAVIEW_I_TE_CAT_DEFINE_TAGS(NAME, ...)                             \
  DEJAVIEW_I_TE_CAT_DEFINE_TAGS_CALL(                                        \
      DEJAVIEW_I_TE_CAT_DEFINE_TAGS_GET_MACRO(                               \
          __VA_ARGS__, DEJAVIEW_I_TE_CAT_DEFINE_TAGS_4,                      \
          DEJAVIEW_I_TE_CAT_DEFINE_TAGS_3, DEJAVIEW_I_TE_CAT_DEFINE_TAGS_2,  \
          DEJAVIEW_I_TE_CAT_DEFINE_TAGS_1, DEJAVIEW_I_TE_CAT_DEFINE_TAGS_0), \
      (NAME, __VA_ARGS__))

#define DEJAVIEW_I_TE_CAT_LIST_TAGS_0(NAME) DEJAVIEW_NULL
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_1(NAME) NAME##_tags
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_2(NAME) NAME##_tags
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_3(NAME) NAME##_tags
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_4(NAME) NAME##_tags
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_GET_MACRO(NAME, A, B, C, D, MACRO, ...) \
  MACRO
#define DEJAVIEW_I_TE_CAT_LIST_TAGS_CALL(MACRO, ARGS) MACRO ARGS
#define DEJAVIEW_I_TE_CAT_LIST_TAGS(NAME, ...)                           \
  DEJAVIEW_I_TE_CAT_LIST_TAGS_CALL(                                      \
      DEJAVIEW_I_TE_CAT_LIST_TAGS_GET_MACRO(                             \
          __VA_ARGS__, DEJAVIEW_I_TE_CAT_LIST_TAGS_4,                    \
          DEJAVIEW_I_TE_CAT_LIST_TAGS_3, DEJAVIEW_I_TE_CAT_LIST_TAGS_2,  \
          DEJAVIEW_I_TE_CAT_LIST_TAGS_1, DEJAVIEW_I_TE_CAT_LIST_TAGS_0), \
      (NAME))

#define DEJAVIEW_I_TE_CAT_DEFINE_CAT(NAME, NAME_STR, ...) \
  DEJAVIEW_I_TE_CAT_DEFINE_TAGS(NAME, __VA_ARGS__)        \
  struct DejaViewTeCategory NAME = {                      \
      &dejaview_atomic_false,                             \
      DEJAVIEW_NULL,                                      \
      {                                                   \
          NAME_STR,                                       \
          DEJAVIEW_I_TE_CAT_DESCRIPTION(__VA_ARGS__),     \
          DEJAVIEW_I_TE_CAT_LIST_TAGS(NAME, __VA_ARGS__), \
          DEJAVIEW_I_TE_CAT_NUM_TAGS(__VA_ARGS__),        \
      },                                                  \
      0,                                                  \
  };

#define DEJAVIEW_I_TE_CAT_DECLARE_CAT(NAME, ...) \
  extern struct DejaViewTeCategory NAME;

#define DEJAVIEW_I_TE_CAT_LIST_CAT_COMMA(NAME, ...) &NAME,

// Declares (without defining) categories as global variables. `MACROS` is used
// to specify the categories, see above for an example.
#define DEJAVIEW_TE_CATEGORIES_DECLARE(MACRO) \
  MACRO(DEJAVIEW_I_TE_CAT_DECLARE_CAT)

// Defines categories as global variables. `MACROS` is used to specify the
// categories, see above for an example.
#define DEJAVIEW_TE_CATEGORIES_DEFINE(MACRO) \
  DEJAVIEW_TE_CATEGORIES_DECLARE(MACRO)      \
  MACRO(DEJAVIEW_I_TE_CAT_DEFINE_CAT)

// Registers categories defined with DEJAVIEW_TE_CATEGORIES_DEFINE(MACRO).
#define DEJAVIEW_TE_REGISTER_CATEGORIES(MACRO)                                 \
  do {                                                                         \
    struct DejaViewTeCategory* dejaview_i_cat_registry[] = {                   \
        MACRO(DEJAVIEW_I_TE_CAT_LIST_CAT_COMMA)};                              \
    DejaViewTeRegisterCategories(                                              \
        dejaview_i_cat_registry,                                               \
        sizeof(dejaview_i_cat_registry) / sizeof(dejaview_i_cat_registry[0])); \
    DejaViewTePublishCategories();                                             \
  } while (0)

// Unregisters categories defined with DEJAVIEW_TE_CATEGORIES_DEFINE(MACRO).
//
// WARNING: The categories cannot be used for tracing anymore after this.
// Executing DEJAVIEW_TE() on unregistered categories will cause a null pointer
// dereference.
#define DEJAVIEW_TE_UNREGISTER_CATEGORIES(MACRO)                               \
  do {                                                                         \
    struct DejaViewTeCategory* dejaview_i_cat_registry[] = {                   \
        MACRO(DEJAVIEW_I_TE_CAT_LIST_CAT_COMMA)};                              \
    DejaViewTeUnregisterCategories(                                            \
        dejaview_i_cat_registry,                                               \
        sizeof(dejaview_i_cat_registry) / sizeof(dejaview_i_cat_registry[0])); \
    DejaViewTePublishCategories();                                             \
  } while (0)

#endif  // INCLUDE_DEJAVIEW_PUBLIC_TE_CATEGORY_MACROS_H_
