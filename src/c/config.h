// config.h - build options.
#pragma once

// 1 = the graph is built in: the solver sheet can graph both sides of a
// one-variable equation, with the roots marked, drag to pan. It's turned on
// and off in the app's Settings. About 4.4 KB of app space and 1.5 KB of heap.
// 0 = left out of the build (roots listed only).
#define SOLVER_GRAPH 1

// 1 = exact answers: equations that are really polynomials (degree <= 4) are
// shown simplified with exact roots like x=(1+sqrt(5))/2, linear x,y systems
// get exact fractions, and the formula view shows sqrt(12) = 2sqrt(3).
// Lives in the math overlay (resources), so it costs no app space.
#define EXACT_FORMS 1

// Length of the buzz when a key registers, in ms (0 = no buzz).
#define KEY_BUZZ_MS 15

