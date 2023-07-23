(set-logic QF_NRA)
(declare-const height Real)
(declare-const weight Real)

;(declare-const height Real GD 1.7 0.05)
;(declare-const weight Real GD 60 5)

(assert (<= weight (* 24.9 height height)))
(assert (>= weight (* 18.5 height height)))
(assert (>= height 2.5))
;(assert (and (>= height 0.9) (<= height 2.3)))
;(assert (and (>= weight 20) (<= weight 200)))
(check-sat)
(get-model)
(exit)