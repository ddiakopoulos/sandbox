#ifndef linear_algebra_h
#define linear_algebra_h

#include "linalg.h"
#include <iostream>

namespace avl
{
    using namespace linalg::aliases;

    template<class T, int M> linalg::vec<T, M>  safe_normalize(const linalg::vec<T,M> & a)                          { return a / std::max(T(1E-6), length(a)); }
    template<class T, int N> linalg::mat<T,N,N> inv(const linalg::mat<T,N,N> & a)                                   { return inverse(a); }

    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T,2> & b) { return a << '[' << b.x << ' ' << b.y << ']'; }
    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T,3> & b) { return a << '[' << b.x << ' ' << b.y << ' ' << b.z << ']'; }
    template<class T> std::ostream & operator << (std::ostream & a, const linalg::vec<T,4> & b) { return a << '[' << b.x << ' ' << b.y << ' ' << b.z << ' ' << b.w << ']'; }

    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T,2,N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T,3,N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n' << b.row(2) << '\n'; }
    template<class T, int N> std::ostream & operator << (std::ostream & a, const linalg::mat<T,4,N> & b) { return a << '\n' << b.row(0) << '\n' << b.row(1) << '\n' << b.row(2) << '\n' << b.row(3) << '\n'; }
}

#endif // end linear_algebra_h