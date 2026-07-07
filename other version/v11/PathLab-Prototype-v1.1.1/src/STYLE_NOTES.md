# Modern C++ cleanup notes

- Kept the public API stable.
- Kept the double-underscore private-member style requested by the project.
- Used `std::array` for fixed algorithm/profile/font/direction tables.
- Replaced repeated movement-rule switches with constexpr lookup tables.
- Added lazy-deletion checks to the priority queue search loop to skip stale heap nodes.
- Reserved animation-frame storage before search to reduce reallocations.
- Kept the code on C++17; no ranges/coroutines/modules, because Visual Studio + raylib projects should not be tortured for no reason.
