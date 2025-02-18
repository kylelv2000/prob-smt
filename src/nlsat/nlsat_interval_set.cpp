/*++
Copyright (c) 2012 Microsoft Corporation

Module Name:

    nlsat_interval_set.cpp

Abstract:

    Sets of disjoint infeasible intervals.

Author:

    Leonardo de Moura (leonardo) 2012-01-11.

Revision History:

--*/
#include "nlsat/nlsat_interval_set.h"
#include "math/polynomial/algebraic_numbers.h"
#include "util/buffer.h"
#include <math.h>

namespace nlsat {
    distribution::distribution(){
        std::uniform_int_distribution<>::param_type new_params(0, RANDOM_PRECISION);
        dis.param(new_params);
    }
    //hr
    distribution::distribution(var index, unsigned type, rational exp, rational var, unsigned ti):
            m_index(index), 
            m_type(type),
            m_exp(exp),
            m_var(var) {
                set_seed(ti);
    }
    void distribution::set_seed(unsigned s) {
        gen.seed(s);
        std::uniform_int_distribution<>::param_type new_params(0, RANDOM_PRECISION);
        dis.param(new_params);
    }
    double distribution::rand_GD(double i, double j) { 
        double u1, u2, r;
        u1 = double(dis(gen)%RANDOM_PRECISION)/RANDOM_PRECISION;
        u2 = double(dis(gen)%RANDOM_PRECISION)/RANDOM_PRECISION;
        // static unsigned int seed = 0; 
        r = i + std::sqrt(j) * std::sqrt(-2.0*(std::log(u1)/std::log(std::exp(1.0)))) * cos(2*PI*u2);
        return r;
    }

    double distribution::rand_UD(double i, double j) { 
        double u1, u2, r;
        u1 = dis(gen) % 2 == 0 ? 1 : -1;
        u2 = double(dis(gen)%RANDOM_PRECISION)*j/RANDOM_PRECISION;
        r = i + u1*u2;
        return r;
    }

    double distribution::Normal(double z) {
        double temp;
        temp = std::exp((-1)*z*z/2)/std::sqrt(2*PI);
        return temp;
    }

    double distribution::NormSDist(const double z) {
        // this guards against overflow
        if (z > 1000000000) return 1;
        if (z < -1000000000) return 0; 
        static const double gamma =  0.231641900, a1  =  0.319381530,
                            a2  = -0.356563782, a3  =  1.781477973,
                            a4  = -1.821255978, a5  =  1.330274429;
        
        double k = 1.0 / (1 + fabs(z) * gamma);
        double n = k * (a1 + k*(a2 + k*(a3 + k*(a4 + k*a5))));
        n = 1 - Normal(z)*n;
        if (z < 0) return 1.0-n; 
        return n;
    }

    double distribution::normsinv(const double p) {
        static const double LOW  = 0.02425;
        static const double HIGH = 0.97575;
        static const double a[] = {
            -3.969683028665376e+01,
            2.209460984245205e+02,
            -2.759285104469687e+02,
            1.383577518672690e+02,
            -3.066479806614716e+01,
            2.506628277459239e+00
        }; 
        static const double b[] = {
            -5.447609879822406e+01,
            1.615858368580409e+02,
            -1.556989798598866e+02,
            6.680131188771972e+01,
            -1.328068155288572e+01
        }; 
        static const double c[] = {
            -7.784894002430293e-03,
            -3.223964580411365e-01,
            -2.400758277161838e+00,
            -2.549732539343734e+00,
            4.374664141464968e+00,
            2.938163982698783e+00
        }; 
        static const double d[] = {
            7.784695709041462e-03,
            3.224671290700398e-01,
            2.445134137142996e+00,
            3.754408661907416e+00
        };
        double q, r;
        errno = 0; 
        
        if (p<0 || p>1) {
            errno = EDOM;
            return 0.0;
        } else if (p == 0) {
            errno = ERANGE;
            return -HUGE_VAL /* minus "infinity" */;
        } else if (p == 1) {
            errno = ERANGE;
            return HUGE_VAL /* "infinity" */;
        } else if (p < LOW) {
            /* Rational approximation for lower region */
            q = sqrt(-2*log(p));
            return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
        } else if (p > HIGH) {
            /* Rational approximation for upper region */
            q  = sqrt(-2*log(1-p));
            return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
        } else {
            /* Rational approximation for central region */
            q = p - 0.5;
            r = q*q;
            return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q / (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
        }
    }
    
    double distribution::CDF(double z) {
        return NormSDist((z-m_exp.get_double())/m_var.get_double());
    }

    double distribution::PPF(double z) {
        return normsinv(z)*m_var.get_double()+m_exp.get_double();
    }
    void distribution::sample(anum_manager & m_am, anum & w) {
        SASSERT(m_type != 0);
        rational result;
        if (m_type == 1) result = rational( to_char(rand_GD(m_exp.get_double(), m_var.get_double())).c_str() );
        else if (m_type == 2) result = rational( to_char(rand_UD(m_exp.get_double(), m_var.get_double())).c_str() );
        TRACE("hr", tout << "sample():" << to_char(rand()) << "\n";);
        m_am.set(w, result.to_mpq());
    }
    void distribution::sample(anum_manager & m_am, anum & w, anum lower, anum upper) {
        SASSERT(m_type != 0);
        double u = double(dis(gen)%(RANDOM_PRECISION-1)+1)/RANDOM_PRECISION; //边界情况可能不满足
        double a = to_double(m_am, lower);
        double b = to_double(m_am, upper);
        // if (a >= b) {
        //     m_am.set(w, lower);
        //     return;
        // }
        rational result;
        if (m_type == 1) result = rational( to_char(PPF( CDF(a) + u*(CDF(b)-CDF(a)) )).c_str() );
        else if (m_type == 2) result = rational( to_char(u*(b-a) + a).c_str() );
        TRACE("hr", tout << "u: " << u << "\n";);
        // if (b == 1) result = rational("0");
        TRACE("hr", tout << "sample(low, upp): " << a << ":" << b << "->" << result << "\n";);
        m_am.set(w, result.to_mpq());
    }

    void distribution::sample(anum_manager & m_am, anum & w, anum lower, anum upper, double rand) {
        SASSERT(m_type != 0);
        double u = rand;
        double a = to_double(m_am, lower);
        double b = to_double(m_am, upper);
        // if (a >= b) {
        //     m_am.set(w, lower);
        //     return;
        // }
        rational result;
        if (m_type == 1) result = rational( to_char(PPF( CDF(a) + u )).c_str() );
        else if (m_type == 2) result = rational( to_char(u*(b-a) + a).c_str() );
        TRACE("hr", tout << "sample(low, upp): " << a << ":" << b << "->" << result << "\n";);
        m_am.set(w, result.to_mpq());
    }

    void distribution::sample(anum_manager & m_am, anum & w, bool has_low, anum bound) {
        SASSERT(m_type != 0);
        double u = double(dis(gen)%(RANDOM_PRECISION-1)+1) / RANDOM_PRECISION; //边界情况可能不满足
        if (has_low) {
            double a = to_double(m_am, bound);
            rational result;
            if (m_type == 1) result = rational( to_char(PPF( CDF(a) + u*(1-CDF(a)) )).c_str() );
            else if (m_type == 2) 
                result = rational( to_char(a + u*m_var.get_double()).c_str() );
            TRACE("hr", tout << "sample(has_low, bound):" << result <<' '<<a<< "\n";);
            m_am.set(w, result.to_mpq());
        } else {
            double b = to_double(m_am, bound);
            rational result;
            if (m_type == 1) result = rational( to_char(PPF( u*(CDF(b)) )).c_str() );
            else if (m_type == 2) 
                result = rational( to_char(b - u*m_var.get_double()).c_str() );
            TRACE("hr", tout << "sample(has_upp, bound):" << result << "\n";);
            m_am.set(w, result.to_mpq());
        }
    }

    void distribution::sample(anum_manager & m_am, anum & w, bool has_low, anum bound, double rand) {
        SASSERT(m_type != 0);
        double u = rand;
        if (has_low) {
            double a = to_double(m_am, bound);
            rational result;
            if (m_type == 1) result = rational( to_char(PPF( CDF(a) + u )).c_str() );
            else if (m_type == 2) 
                result = rational( to_char(a + u*m_var.get_double()).c_str() );
            TRACE("hr", tout << "sample(has_low, bound):" << result << "\n";);
            m_am.set(w, result.to_mpq());
        } else {
            double b = to_double(m_am, bound);
            rational result;
            if (m_type == 1) result = rational( to_char(PPF( u )).c_str() );
            else if (m_type == 2) 
                result = rational( to_char(b - u*m_var.get_double()).c_str() );
            TRACE("hr", tout << "sample(has_upp, bound):" << result << "\n";);
            m_am.set(w, result.to_mpq());
        }
    }

    double distribution::get_prob(anum_manager & m_am, anum point) {
        double result;
        if (m_type == 1) {
            double loc = to_double(m_am, point);
            double exp = m_exp.get_double();
            double var = m_var.get_double();
            result = std::exp((-1)*(loc-exp)*(loc-exp)/(2*var*var))/(std::sqrt(2*PI)*var);
        } else if (m_type == 2) {
            double var = m_var.get_double();
            result = 1.0/RANDOM_PRECISION;
        }
        return result;
    }

    double distribution::get_prob(anum_manager & m_am, anum lower, anum upper) {
        double result;
        double a = to_double(m_am, lower);
        double b = to_double(m_am, upper);
        if (m_type == 1) result = CDF(b) - CDF(a);
        else if (m_type == 2) result = b-a;
        return result;
    }

    double distribution::get_prob(anum_manager & m_am, bool has_low, anum point) {
        double result;
        if (has_low) {
            double a = to_double(m_am, point);
            if (m_type == 1) return 1 - CDF(a);
            else if (m_type == 2) result = m_var.get_double();
        } else {
            double b = to_double(m_am, point);
            if (m_type == 1) return CDF(b);
            else if (m_type == 2) result = m_var.get_double();
        }
        return result;
    }
    
    double distribution::to_double(anum_manager & m_am, anum input) {
        std::stringstream str;
        m_am.display_decimal(str, input);
        // TRACE("hr", tout << "display_decimal: " << atof(str.str().c_str()););
        return atof(str.str().c_str());
    }

    std::string distribution::to_char(double input) {
        std::string tmp = std::to_string(input);
        return tmp;
        // return tmp.c_str();
    }

    struct interval {
        unsigned  m_lower_open:1;
        unsigned  m_upper_open:1;
        unsigned  m_lower_inf:1;
        unsigned  m_upper_inf:1;
        literal   m_justification;
        clause const* m_clause;
        anum      m_lower;
        anum      m_upper;
    };

    class interval_set {
    public:
        static unsigned get_obj_size(unsigned num) { return sizeof(interval_set) + num*sizeof(interval); }
        unsigned  m_num_intervals;
        unsigned  m_ref_count:31;
        unsigned  m_full:1;
        interval  m_intervals[0];
    };

    void display(std::ostream & out, anum_manager & am, interval const & curr) {
        if (curr.m_lower_inf) {
            out << "(-oo, ";
        }
        else {
            if (curr.m_lower_open)
                    out << "(";
            else
                out << "[";
            am.display_decimal(out, curr.m_lower);
            out << ", ";
        }
        if (curr.m_justification.sign())
            out << "~";
        out << "p";
        out << curr.m_justification.var() << ", ";
        if (curr.m_upper_inf) {
            out << "oo)";
        }
        else {
            am.display_decimal(out, curr.m_upper);
            if (curr.m_upper_open)
                out << ")";
            else
                out << "]";
        }
    }

    bool check_interval(anum_manager & am, interval const & i) {
        SASSERT(!i.m_lower_inf || i.m_lower_open);        
        SASSERT(!i.m_upper_inf || i.m_upper_open);
        
        if (!i.m_lower_inf && !i.m_upper_inf) {
            auto s = am.compare(i.m_lower, i.m_upper);
            (void)s;
            TRACE("nlsat_interval", tout << "lower: "; am.display_decimal(tout, i.m_lower); tout << ", upper: "; am.display_decimal(tout, i.m_upper);
                  tout << "\ns: " << s << "\n";);
            SASSERT(s <= 0);
            SASSERT(!is_zero(s) || (!i.m_lower_open && !i.m_upper_open));
        }
        return true;
    }

    bool check_no_overlap(anum_manager & am, interval const & curr, interval const & next) {
        SASSERT(!curr.m_upper_inf);
        SASSERT(!next.m_lower_inf);
        sign s = am.compare(curr.m_upper, next.m_lower);
        CTRACE("nlsat", s > 0, display(tout, am, curr); tout << "  "; display(tout, am, next); tout << "\n";);
        SASSERT(s <= 0);
        SASSERT(!is_zero(s) || curr.m_upper_open || next.m_lower_open);        
        (void)s;
        return true;
    }
    
    // Check if the intervals are valid, ordered, and are disjoint.
    bool check_interval_set(anum_manager & am, unsigned sz, interval const * ints) {
        DEBUG_CODE(
            for (unsigned i = 0; i < sz; i++) {
                interval const & curr = ints[i];
                SASSERT(check_interval(am, curr));
                SASSERT(i >= sz - 1 || check_no_overlap(am, curr, ints[i+1]));                
            });
        return true;
    }

    interval_set_manager::interval_set_manager(anum_manager & m, small_object_allocator & a):
        m_am(m),
        m_allocator(a) {
    }
     
    interval_set_manager::~interval_set_manager() {
    }
    
    void interval_set_manager::del(interval_set * s) {
        if (s == nullptr)
            return;
        unsigned num    = s->m_num_intervals;
        unsigned obj_sz = interval_set::get_obj_size(num);
        for (unsigned i = 0; i < num; i++) {
            m_am.del(s->m_intervals[i].m_lower);
            m_am.del(s->m_intervals[i].m_upper);
        }
        s->~interval_set();
        m_allocator.deallocate(obj_sz, s);
    }

    void interval_set_manager::dec_ref(interval_set * s) {
        SASSERT(s->m_ref_count > 0);
        s->m_ref_count--;
        if (s->m_ref_count == 0)
            del(s);
    }

    void interval_set_manager::inc_ref(interval_set * s) {
        s->m_ref_count++;
    }
    
    interval_set * interval_set_manager::mk(bool lower_open, bool lower_inf, anum const & lower, 
                                            bool upper_open, bool upper_inf, anum const & upper,
                                            literal justification, clause const* cls) {
        void * mem = m_allocator.allocate(interval_set::get_obj_size(1));
        interval_set * new_set = new (mem) interval_set();
        new_set->m_num_intervals = 1;
        new_set->m_ref_count  = 0;
        new_set->m_full       = lower_inf && upper_inf;
        interval * i = new (new_set->m_intervals) interval();
        i->m_lower_open = lower_open;
        i->m_lower_inf  = lower_inf;
        i->m_upper_open = upper_open;
        i->m_upper_inf  = upper_inf;
        i->m_justification = justification;
        i->m_clause = cls;
        if (!lower_inf)
            m_am.set(i->m_lower, lower);
        if (!upper_inf)
            m_am.set(i->m_upper, upper);
        SASSERT(check_interval_set(m_am, 1, new_set->m_intervals));
        return new_set;
    }

    inline ::sign compare_lower_lower(anum_manager & am, interval const & i1, interval const & i2) {
        if (i1.m_lower_inf && i2.m_lower_inf)
            return sign_zero;
        if (i1.m_lower_inf)
            return sign_neg;
        if (i2.m_lower_inf)
            return sign_pos;
        SASSERT(!i1.m_lower_inf && !i2.m_lower_inf);
        ::sign s = am.compare(i1.m_lower, i2.m_lower);
        if (!is_zero(s)) 
            return s;
        if (i1.m_lower_open == i2.m_lower_open)
            return sign_zero;
        if (i1.m_lower_open)
            return sign_pos;
        else
            return sign_neg;
    }

    inline ::sign compare_upper_upper(anum_manager & am, interval const & i1, interval const & i2) {
        if (i1.m_upper_inf && i2.m_upper_inf)
            return sign_zero;
        if (i1.m_upper_inf)
            return sign_pos;
        if (i2.m_upper_inf)
            return sign_neg;
        SASSERT(!i1.m_upper_inf && !i2.m_upper_inf);
        auto s = am.compare(i1.m_upper, i2.m_upper);
        if (!::is_zero(s)) 
            return s;
        if (i1.m_upper_open == i2.m_upper_open)
            return sign_zero;
        if (i1.m_upper_open)
            return sign_neg;
        else
            return sign_pos;
    }

    inline ::sign compare_upper_lower(anum_manager & am, interval const & i1, interval const & i2) {
        if (i1.m_upper_inf || i2.m_lower_inf) {
            TRACE("nlsat_interval", nlsat::display(tout << "i1: ", am, i1); nlsat::display(tout << "i2: ", am, i2););
            return sign_pos;
        }
        SASSERT(!i1.m_upper_inf && !i2.m_lower_inf);
        auto s = am.compare(i1.m_upper, i2.m_lower);
        TRACE("nlsat_interval", nlsat::display(tout << "i1: ", am, i1); nlsat::display(tout << " i2: ", am, i2); 
              tout << " compare: " << s << "\n";);
        if (!::is_zero(s))
            return s;
        if (!i1.m_upper_open && !i2.m_lower_open)
            return sign_zero;
        return sign_neg;
    }
    
    typedef sbuffer<interval, 128> interval_buffer;

    // Given two interval in an interval set s.t. curr occurs before next.
    // We say curr and next are "adjacent" iff
    //      there is no "space" between them.
    bool adjacent(anum_manager & am, interval const & curr, interval const & next) {
        SASSERT(!curr.m_upper_inf);
        SASSERT(!next.m_lower_inf);
        auto sign = am.compare(curr.m_upper, next.m_lower);
        SASSERT(sign != sign_pos);
        if (is_zero(sign)) {
            SASSERT(curr.m_upper_open || next.m_lower_open);
            return !curr.m_upper_open || !next.m_lower_open;
        }
        return false;
    }

    inline void push_back(anum_manager & am, interval_buffer & buf, 
                          bool lower_open, bool lower_inf, anum const & lower, 
                          bool upper_open, bool upper_inf, anum const & upper,
                          literal justification) {
        buf.push_back(interval());
        interval & i = buf.back();
        i.m_lower_open = lower_open;
        i.m_lower_inf  = lower_inf;
        am.set(i.m_lower, lower);
        i.m_upper_open = upper_open;
        i.m_upper_inf  = upper_inf;
        am.set(i.m_upper, upper);
        i.m_justification = justification;
        SASSERT(check_interval(am, i));
    }

    inline void push_back(anum_manager & am, interval_buffer & buf, interval const & i) {
        push_back(am, buf,
                  i.m_lower_open, i.m_lower_inf, i.m_lower,
                  i.m_upper_open, i.m_upper_inf, i.m_upper,
                  i.m_justification);
    }

    inline interval_set * mk_interval(small_object_allocator & allocator, interval_buffer & buf, bool full) {
        unsigned sz = buf.size();
        void * mem = allocator.allocate(interval_set::get_obj_size(sz));
        interval_set * new_set = new (mem) interval_set();
        new_set->m_full = full;
        new_set->m_ref_count  = 0;
        new_set->m_num_intervals = sz;
        memcpy(new_set->m_intervals, buf.data(), sizeof(interval)*sz);
        return new_set;
    }

    interval_set * interval_set_manager::mk_union(interval_set const * s1, interval_set const * s2) {
#if 0
        static unsigned s_count = 0;
        s_count++;
        if (s_count == 4470) {
            enable_trace("nlsat_interval");
            enable_trace("algebraic");
        }
#endif
        TRACE("nlsat_interval", tout << "mk_union\ns1: "; display(tout, s1); tout << "\ns2: "; display(tout, s2); tout << "\n";);
        if (s1 == nullptr || s1 == s2)
            return const_cast<interval_set*>(s2);
        if (s2 == nullptr)
            return const_cast<interval_set*>(s1);
        if (s1->m_full)
            return const_cast<interval_set*>(s1);
        if (s2->m_full)
            return const_cast<interval_set*>(s2);
        interval_buffer result;
        unsigned sz1 = s1->m_num_intervals;
        unsigned sz2 = s2->m_num_intervals;
        unsigned i1  = 0;
        unsigned i2  = 0;
        while (true) {
            if (i1 >= sz1) {
                while (i2 < sz2) {
                    TRACE("nlsat_interval", tout << "adding remaining intervals from s2: "; nlsat::display(tout, m_am, s2->m_intervals[i2]); tout << "\n";);
                    push_back(m_am, result, s2->m_intervals[i2]);
                    i2++;
                }
                break;
            }
            if (i2 >= sz2) {
                while (i1 < sz1) {
                    TRACE("nlsat_interval", tout << "adding remaining intervals from s1: "; nlsat::display(tout, m_am, s1->m_intervals[i1]); tout << "\n";);
                    push_back(m_am, result, s1->m_intervals[i1]);
                    i1++;
                }
                break;
            }
            interval const & int1 = s1->m_intervals[i1];
            interval const & int2 = s2->m_intervals[i2];
            int l1_l2_sign = compare_lower_lower(m_am, int1, int2);
            int u1_u2_sign = compare_upper_upper(m_am, int1, int2);
            TRACE("nlsat_interval", 
                  tout << "i1: " << i1 << ", i2: " << i2 << "\n";
                  tout << "int1: "; nlsat::display(tout, m_am, int1); tout << "\n";
                  tout << "int2: "; nlsat::display(tout, m_am, int2); tout << "\n";);
            if (l1_l2_sign <= 0) {
                if (u1_u2_sign == 0) {
                    // Cases:
                    // 1)  [     ]
                    //     [     ]
                    //
                    // 2) [     ]
                    //      [   ]
                    //
                    TRACE("nlsat_interval", tout << "l1_l2_sign <= 0, u1_u2_sign == 0\n";);
                    push_back(m_am, result, int1);
                    i1++;
                    i2++;
                }
                else if (u1_u2_sign > 0) {
                    // Cases:
                    //
                    // 1) [        ]
                    //    [     ]
                    //
                    // 2) [        ]
                    //      [   ]
                    i2++;
                    TRACE("nlsat_interval", tout << "l1_l2_sign <= 0, u1_u2_sign > 0\n";);
                    // i1 may consume other intervals of s2
                }
                else {
                    SASSERT(u1_u2_sign < 0);
                    int u1_l2_sign = compare_upper_lower(m_am, int1, int2);
                    if (u1_l2_sign < 0) {
                        SASSERT(l1_l2_sign < 0);
                        // Cases:
                        // 1)   [      ]
                        //                [     ]
                        TRACE("nlsat_interval", tout << "l1_l2_sign <= 0, u1_u2_sign < 0, u1_l2_sign < 0\n";);
                        push_back(m_am, result, int1);
                        i1++;
                    }
                    else if (u1_l2_sign == 0) {
                        SASSERT(l1_l2_sign <= 0);
                        SASSERT(!int1.m_upper_open && !int2.m_lower_open);
                        SASSERT(!int2.m_lower_inf);
                        TRACE("nlsat_interval", tout << "l1_l2_sign <= 0, u1_u2_sign < 0, u1_l2_sign == 0\n";);
                        // Cases:
                        if (l1_l2_sign != 0) {
                            SASSERT(l1_l2_sign < 0);
                            // 1)   [   ]
                            //          [    ]
                            SASSERT(!int2.m_lower_open);
                            push_back(m_am, result, 
                                      int1.m_lower_open, int1.m_lower_inf,  int1.m_lower,
                                      true /* open */, false /* not +oo */, int1.m_upper, 
                                      int1.m_justification); 
                            i1++;
                        }
                        else {
                            SASSERT(l1_l2_sign == 0);
                            // 2)   u          <<< int1 is a singleton
                            //      [     ]
                            // just consume int1
                            i1++;
                        }
                    }
                    else {
                        SASSERT(l1_l2_sign <= 0);
                        SASSERT(u1_u2_sign < 0);
                        SASSERT(u1_l2_sign > 0);
                        TRACE("nlsat_interval", tout << "l1_l2_sign <= 0, u1_u2_sign < 0, u1_l2_sign > 0\n";);
                        if (l1_l2_sign == 0) {
                            // Case:
                            // 1)   [      ]
                            //      [          ]
                            // just consume int1
                            i1++;
                        }
                        else {
                            SASSERT(l1_l2_sign < 0);
                            SASSERT(u1_u2_sign < 0);
                            SASSERT(u1_l2_sign > 0);
                            // 2) [        ]
                            //         [        ]
                            push_back(m_am, result, 
                                      int1.m_lower_open,     int1.m_lower_inf, int1.m_lower,
                                      !int2.m_lower_open, false /* not +oo */, int2.m_lower,
                                      int1.m_justification); 
                            i1++;
                        }
                    }
                }
            }
            else {
                SASSERT(l1_l2_sign > 0);
                if (u1_u2_sign == 0) {
                    TRACE("nlsat_interval", tout << "l2 < l1 <= u1 = u2\n";);
                    // Case:
                    // 1)    [  ]
                    //    [     ]
                    //
                    push_back(m_am, result, int2);
                    i1++;
                    i2++;
                }
                else if (u1_u2_sign < 0) {
                    TRACE("nlsat_interval", tout << "l2 < l1 <= u2 < u2\n";);
                    // Case:
                    // 1)   [   ]
                    //    [       ]
                    i1++;
                    // i2 may consume other intervals of s1
                }
                else {
                    auto u2_l1_sign = compare_upper_lower(m_am, int2, int1);
                    if (u2_l1_sign < 0) {
                        TRACE("nlsat_interval", tout << "l2 <= u2 < l1 <= u1\n";);
                        // Case:
                        // 1)           [      ]
                        //     [     ]
                        push_back(m_am, result, int2);
                        i2++;
                    }
                    else if (is_zero(u2_l1_sign)) {
                        TRACE("nlsat_interval", tout << "l1_l2_sign > 0, u1_u2_sign > 0, u2_l1_sign == 0\n";);
                        SASSERT(!int1.m_lower_open && !int2.m_upper_open);
                        SASSERT(!int1.m_lower_inf);
                        // Case:
                        //      [    ]   
                        //  [   ]
                        push_back(m_am, result, 
                                  int2.m_lower_open,     int2.m_lower_inf, int2.m_lower,
                                  true /* open */,    false /* not +oo */, int2.m_upper, 
                                  int2.m_justification); 
                        i2++;
                    }
                    else {
                        TRACE("nlsat_interval", tout << "l2 < l1 < u2 < u1\n";);
                        SASSERT(l1_l2_sign > 0);
                        SASSERT(u1_u2_sign > 0);
                        SASSERT(u2_l1_sign > 0);
                        // Case:
                        //     [        ]
                        // [        ]
                        push_back(m_am, result, 
                                  int2.m_lower_open,     int2.m_lower_inf, int2.m_lower,
                                  !int1.m_lower_open, false /* not +oo */, int1.m_lower,
                                  int2.m_justification); 
                        i2++;
                    }
                }
            }
            SASSERT(result.size() <= 1 ||
                    check_no_overlap(m_am, result[result.size() - 2], result[result.size() - 1]));

        }

        SASSERT(!result.empty());
        SASSERT(check_interval_set(m_am, result.size(), result.data()));
        // Compress
        // Remark: we only combine adjacent intervals when they have the same justification
        unsigned j  = 0;
        unsigned sz = result.size(); 
        for (unsigned i = 1; i < sz; i++) {
            interval & curr = result[j];
            interval & next = result[i];
            if (curr.m_justification == next.m_justification && 
                adjacent(m_am, curr, next)) {
                // merge them
                curr.m_upper_inf  = next.m_upper_inf;
                curr.m_upper_open = next.m_upper_open;
                m_am.swap(curr.m_upper, next.m_upper);
            }
            else {
                j++;
                if (i != j) {
                    interval & next_curr = result[j];
                    next_curr.m_lower_inf = next.m_lower_inf;
                    next_curr.m_lower_open = next.m_lower_open;
                    m_am.swap(next_curr.m_lower, next.m_lower);
                    next_curr.m_upper_inf = next.m_upper_inf;
                    next_curr.m_upper_open = next.m_upper_open;
                    m_am.swap(next_curr.m_upper, next.m_upper);
                    next_curr.m_justification = next.m_justification;
                }
            }
        }
        j++;
        for (unsigned i = j; i < sz; i++) {
            interval & curr = result[i];
            m_am.del(curr.m_lower);
            m_am.del(curr.m_upper);
        }
        result.shrink(j);
        SASSERT(check_interval_set(m_am, result.size(), result.data()));
        sz = j;
        SASSERT(sz >= 1);
        bool found_slack  = !result[0].m_lower_inf || !result[sz-1].m_upper_inf;
        // Check if full
        for (unsigned i = 0; i < sz - 1 && !found_slack; i++) {
            if (!adjacent(m_am, result[i], result[i+1]))
                found_slack = true;
        }
        // Create new interval set
        interval_set * new_set = mk_interval(m_allocator, result, !found_slack);
        SASSERT(check_interval_set(m_am, sz, new_set->m_intervals));
        return new_set;
    }

    bool interval_set_manager::is_full(interval_set const * s) {
        if (s == nullptr)
            return false;
        return s->m_full == 1;
    }

    unsigned interval_set_manager::num_intervals(interval_set const * s) const {
        if (s == nullptr) return 0;
        return s->m_num_intervals;
    }
    
    bool interval_set_manager::subset(interval_set const * s1, interval_set const * s2) {
        if (s1 == s2)
            return true;
        if (s1 == nullptr)
            return true;
        if (s2 == nullptr)
            return false;
        if (s2->m_full)
            return true;
        if (s1->m_full)
            return false;
        unsigned sz1 = s1->m_num_intervals;
        unsigned sz2 = s2->m_num_intervals;
        SASSERT(sz1 > 0 && sz2 > 0);
        unsigned i1  = 0;
        unsigned i2  = 0;
        while (i1 < sz1 && i2 < sz2) {
            interval const & int1 = s1->m_intervals[i1];
            interval const & int2 = s2->m_intervals[i2];
            TRACE("nlsat_interval", tout << "subset main loop, i1: " << i1 << ", i2: " << i2 << "\n";
                  tout << "int1: "; nlsat::display(tout, m_am, int1); tout << "\n";
                  tout << "int2: "; nlsat::display(tout, m_am, int2); tout << "\n";);
            if (compare_lower_lower(m_am, int1, int2) < 0) {
                TRACE("nlsat_interval", tout << "done\n";);
                // interval [int1.lower1, int2.lower2] is not in s2
                // s1: [ ...
                // s2:    [ ...
                return false;
            }
            while (i2 < sz2) {
                interval const & int2 = s2->m_intervals[i2];
                TRACE("nlsat_interval", tout << "inner loop, i2: " << i2 << "\n";
                      tout << "int2: "; nlsat::display(tout, m_am, int2); tout << "\n";);
                int u1_u2_sign = compare_upper_upper(m_am, int1, int2);
                if (u1_u2_sign == 0) {
                    TRACE("nlsat_interval", tout << "case 1, break\n";);
                    // consume both
                    // s1: ... ]
                    // s2: ... ]
                    i1++;
                    i2++;
                    break;
                }
                else if (u1_u2_sign < 0) {
                    TRACE("nlsat_interval", tout << "case 2, break\n";);
                    // consume only int1, int2 may cover other intervals of s1 
                    // s1:  ... ]
                    // s2:    ... ]
                    i1++;
                    break;
                }
                else {
                    SASSERT(u1_u2_sign > 0);
                    int u2_l1_sign = compare_upper_lower(m_am, int2, int1);
                    TRACE("nlsat_interval", tout << "subset, u2_l1_sign: " << u2_l1_sign << "\n";);
                    if (u2_l1_sign < 0) {
                        TRACE("nlsat_interval", tout << "case 3, break\n";);
                        // s1:           [ ...
                        // s2: [ ... ]  ...
                        i2++;
                        break;
                    }
                    SASSERT(u2_l1_sign >= 0);
                    // s1:      [ ...  ]
                    // s2: [ ...    ]
                    if (i2 == sz2 - 1) {
                        TRACE("nlsat_interval", tout << "case 4, done\n";);
                        // s1:   ... ]
                        // s2: ...]
                        // the interval [int2.upper, int1.upper] is not in s2
                        return false; // last interval of s2
                    }
                    interval const & next2 = s2->m_intervals[i2+1];
                    if (!adjacent(m_am, int2, next2)) {
                        TRACE("nlsat_interval", tout << "not adjacent, done\n";);
                        // s1:   ... ]
                        // s2: ... ]   [
                        // the interval [int2.upper, min(int1.upper, next2.lower)] is not in s2
                        return false;
                    }
                    TRACE("nlsat_interval", tout << "continue..\n";);
                    // continue with adjacent interval of s2
                    // s1:    ...  ]
                    // s2:  ..][ ...
                    i2++;
                }
            }
        }
        return i1 == sz1;
    }

    bool interval_set_manager::set_eq(interval_set const * s1, interval_set const * s2) {
        if (s1 == nullptr || s2 == nullptr)
            return s1 == s2;
        if (s1->m_full || s2->m_full)
            return s1->m_full == s2->m_full;
        // TODO: check if bottleneck, then replace simple implementation
        return subset(s1, s2) && subset(s2, s1);
    }
        
    bool interval_set_manager::eq(interval_set const * s1, interval_set const * s2) {
        if (s1 == nullptr || s2 == nullptr)
            return s1 == s2;
        if (s1->m_num_intervals != s2->m_num_intervals)
            return false;
        for (unsigned i = 0; i < s1->m_num_intervals; i++) {
            interval const & int1 = s1->m_intervals[i];
            interval const & int2 = s2->m_intervals[i]; 
            if (int1.m_lower_inf  != int2.m_lower_inf ||
                int1.m_lower_open != int2.m_lower_open ||
                int1.m_upper_inf  != int2.m_upper_inf ||
                int1.m_upper_open != int2.m_upper_open ||
                int1.m_justification != int2.m_justification ||
                !m_am.eq(int1.m_lower, int2.m_lower) ||
                !m_am.eq(int1.m_upper, int2.m_upper))
                return false;
        }
        return true;
    }

    void interval_set_manager::get_justifications(interval_set const * s, literal_vector & js, ptr_vector<clause>& clauses) {
        js.reset();
        clauses.reset();
        unsigned num = num_intervals(s);
        for (unsigned i = 0; i < num; i++) {
            literal l     = s->m_intervals[i].m_justification;
            unsigned lidx = l.index();
            if (m_already_visited.get(lidx, false))
                continue;
            m_already_visited.setx(lidx, true, false);
            js.push_back(l);
            if (s->m_intervals[i].m_clause) {
                clauses.push_back(const_cast<clause*>(s->m_intervals[i].m_clause));
            }
        }
        for (unsigned i = 0; i < num; i++) {
            literal l     = s->m_intervals[i].m_justification;
            unsigned lidx = l.index();
            m_already_visited[lidx] = false;
        }
    }
    
    interval_set * interval_set_manager::get_interval(interval_set const * s, unsigned idx) const {
        SASSERT(idx < num_intervals(s));
        interval_buffer result;
        push_back(m_am, result, s->m_intervals[idx]);
        bool found_slack  = !result[0].m_lower_inf || !result[0].m_upper_inf;
        interval_set * new_set = mk_interval(m_allocator, result, !found_slack);
        SASSERT(check_interval_set(m_am, result.size(), new_set->m_intervals));
        return new_set;
    }

    void interval_set_manager::peek_in_complement(interval_set const * s, bool is_int, anum & w, bool randomize) {
        SASSERT(!is_full(s));
        if (s == nullptr) {
            if (randomize) {
                int num = m_rand() % 2 == 0 ? 1 : -1;
#define MAX_RANDOM_DEN_K 4
                int den_k = (m_rand() % MAX_RANDOM_DEN_K);
                int den   = is_int ? 1 : (1 << den_k);
                scoped_mpq _w(m_am.qm());
                m_am.qm().set(_w, num, den);
                m_am.set(w, _w);
                return;
            }
            else {
                m_am.set(w, 0);
                return;
            }
        }
        
        unsigned n = 0;
        
        unsigned num = num_intervals(s);
        if (!s->m_intervals[0].m_lower_inf) {
            // lower is not -oo
            n++;
            m_am.int_lt(s->m_intervals[0].m_lower, w);
            if (!randomize)
                return;
        }
        if (!s->m_intervals[num-1].m_upper_inf) {
            // upper is not oo
            n++;
            if (n == 1 || m_rand()%n == 0)
                m_am.int_gt(s->m_intervals[num-1].m_upper, w);
            if (!randomize)
                return;
        }
        
        SASSERT(false);

        // Try to find a gap that is not an unit.
        for (unsigned i = 1; i < num; i++) {
            if (m_am.lt(s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower)) {
                n++;
                if (n == 1 || m_rand()%n == 0)
                    m_am.select(s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower, w);
                if (!randomize)
                    return;
            }
        }
        
        if (n > 0)
            return;
        
        // Try to find a rational
        unsigned irrational_i = UINT_MAX;
        for (unsigned i = 1; i < num; i++) {
            if (s->m_intervals[i-1].m_upper_open && s->m_intervals[i].m_lower_open) {
                SASSERT(m_am.eq(s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower)); // otherwise we would have found it in the previous step
                if (m_am.is_rational(s->m_intervals[i-1].m_upper)) {
                    m_am.set(w, s->m_intervals[i-1].m_upper);
                    return;
                }
                if (irrational_i == UINT_MAX)
                    irrational_i = i-1;
            }
        }
        SASSERT(irrational_i != UINT_MAX);
        // Last option: peek irrational witness :-(
        SASSERT(s->m_intervals[irrational_i].m_upper_open && s->m_intervals[irrational_i+1].m_lower_open);
        m_am.set(w, s->m_intervals[irrational_i].m_upper);
    }
    
    // hr
    void interval_set_manager::peek_in_complement(interval_set const * s, bool is_int, anum & w, distribution& distribution) {
        SASSERT(!is_full(s));
        if (s == nullptr) {
            distribution.sample(m_am, w);
            return;
        }
        unsigned num = num_intervals(s);
        if (num == 1) {
            if (s->m_intervals[0].m_lower_inf) {
                distribution.sample(m_am, w, true, s->m_intervals[0].m_upper);
                return;
            } else if (s->m_intervals[0].m_upper_inf) {
                distribution.sample(m_am, w, false, s->m_intervals[0].m_lower);
                return;
            }
        }

        double* prob = new double[num+1];
        double prob_total = 0;
        if (!s->m_intervals[0].m_lower_inf) {
            prob[0] = distribution.get_prob(m_am, false, s->m_intervals[0].m_lower);
            prob_total += prob[0];
        } else prob[0] = 0;
        if (!s->m_intervals[num-1].m_upper_inf) {
            prob[num] = distribution.get_prob(m_am, true, s->m_intervals[num-1].m_upper);
            prob_total += prob[num];
        } else prob[num] = 0;
        for (unsigned i=1; i<num; i++) {
            if (m_am.lt(s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower)) {
                prob[i] = distribution.get_prob(m_am, s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower);
                prob_total += prob[i];
            } else {
                prob[i] = 0; // 优先考虑区间
            }
        }
// #define RANDOM_PRECISION 10000
        if (prob_total != 0) {
            double rand = (distribution.dis(distribution.gen)%distribution.RANDOM_PRECISION)*prob_total/distribution.RANDOM_PRECISION;
            unsigned index = 0;
            while (rand-prob[index] > 0 || prob[index] == 0) rand -= prob[index++];
            if (index == 0) {
                SASSERT(!s->m_intervals[0].m_lower_inf);
                distribution.sample(m_am, w, false, s->m_intervals[0].m_lower);
                // distribution.sample(m_am, w, false, s->m_intervals[0].m_lower, rand);
            } else if (index == num) {
                SASSERT(!s->m_intervals[num-1].m_upper_inf);
                distribution.sample(m_am, w, true, s->m_intervals[num-1].m_upper);
                // distribution.sample(m_am, w, true, s->m_intervals[num-1].m_upper, rand);
            } else {
                distribution.sample(m_am, w, s->m_intervals[index-1].m_upper, s->m_intervals[index].m_lower);
                // distribution.sample(m_am, w, s->m_intervals[index-1].m_upper, s->m_intervals[index].m_lower, rand);
            }
            delete [] prob;
            return;
        }
        // Try to find a rational
        double prob_opt = 0;
        unsigned irrational_i = UINT_MAX;
        for (unsigned i = 1; i < num; i++) {
            if (s->m_intervals[i-1].m_upper_open && s->m_intervals[i].m_lower_open) {
                SASSERT(m_am.eq(s->m_intervals[i-1].m_upper, s->m_intervals[i].m_lower));
                if (m_am.is_rational(s->m_intervals[i-1].m_upper)) {
                    double prob_cur = distribution.get_prob(m_am, s->m_intervals[i-1].m_upper);
                    if (prob_cur > prob_opt) {
                        prob_opt = prob_cur;
                        m_am.set(w, s->m_intervals[i-1].m_upper);
                    }
                }
                if (irrational_i == UINT_MAX) irrational_i = i-1;
            }
        }
        if (prob_opt != 0) return;
        SASSERT(irrational_i != UINT_MAX);
        // Last option: peek irrational witness :-(
        SASSERT(s->m_intervals[irrational_i].m_upper_open && s->m_intervals[irrational_i+1].m_lower_open);
        m_am.set(w, s->m_intervals[irrational_i].m_upper);
    }

    std::ostream& interval_set_manager::display(std::ostream & out, interval_set const * s) const {
        if (s == nullptr) {
            out << "{}";
            return out;
        }
        out << "{";
        for (unsigned i = 0; i < s->m_num_intervals; i++) {
            if (i > 0)
                out << ", ";
            nlsat::display(out, m_am, s->m_intervals[i]);
        }
        out << "}";
        if (s->m_full)
            out << "*";
        return out;
    }

};
