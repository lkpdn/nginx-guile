;
; Cache::Memcached::Fast style ketama consistent hashing
;
(import (ice-9 optargs))
(import (rnrs bytevectors))

(define POLY32 #xedb88320)
(define KETAMA-POINTS 150)
(define BUCKET '())

(define (make-crc32-procedure table def-bv)
  (let ((default (bytevector-u32-ref def-bv 0 (endianness little))))
    (lambda (data . opt)
      (sint-list->bytevector (list
        (logxor default
          (let* ((r (let-optional opt ((r def-bv)) r))
                 (r-int (lognot (bytevector-u32-ref r 0 (endianness little))))
                 (len (bytevector-length data)))
            (do ((i 0 (+ i 1)))
                ((= i len))
                 (set! r-int
                        (logxor
                         (ash r-int -8)
                         (vector-ref table
                                     (logxor
                                      (logand r-int 255)
                                      (bytevector-u8-ref data i))))))
            r-int))) 'little 4))))

(define (gen-lookup poly)
  (let ((table (make-vector 256)))
    (do ((i 0 (+ i 1)))
        ((= i 256))
      (vector-set! table i
        (do ((j 0 (+ j 1))
             (r i
                ((if (zero? (logand r 1))
                     values
                     (lambda(x) (logxor poly x)))
                 (ash r -1))))
            ((= j 8) r))))
    table))

(define load-val (lambda(idx) (car (list-ref BUCKET idx))))

(define (dispatch-find-bucket point)
  (let ((beg 0)
        (mid 0)
        (end (length BUCKET)))
     (begin
       (if (not (null? BUCKET))
         (while (not (= beg end))
           (let ((mid (floor (/ (+ beg end) 2)))
                 (outer-break break))
             (cond ((< (load-val mid) point)
                     (set! beg (+ mid 1)))
                   ((> (load-val mid) point)
                     (set! end mid))
                   ((= mid 0)
                     (break))
                   (else (begin
                           (while (and (not (= beg mid))
                                       (= (load-val (- mid 1)) point))
                             (set! mid (- mid 1)))
                           (outer-break)))))))
       (if (= beg end) 0 mid))))

(define (insert-bucket-entry idx entry)
  (cond
    ((null? BUCKET) (set! BUCKET (list entry)))
    ((<= idx 0)
      (set! BUCKET (cons entry BUCKET)))
    ((>= idx (length BUCKET))
      (set! BUCKET (append BUCKET (list entry))))
    (else
      (set! BUCKET
        (append (list-head BUCKET (- idx 1))
          (cons entry
            (list-tail BUCKET (- idx 1))))))))

(define (add-server host delim port weight)
  (let* ((count (floor (+ (* weight KETAMA-POINTS) (/ 1 2))))
         (compute-crc32 (make-crc32-procedure
                        (gen-lookup POLY32) #vu8(0 0 0 0)))
         (crc32 (compute-crc32
                  (string->utf8 port)
                  (compute-crc32
                    (string->utf8 delim)
                    (compute-crc32
                      (string->utf8 host))))))
    (do ((i 0 (+ i 1))
         (point crc32 (compute-crc32 crc32 point)))
        ((= i count))
      (let* ((point-u32 (bytevector-u32-ref point 0 (endianness little)))
             (p (dispatch-find-bucket point-u32)))
        (begin
          (if (not (null? BUCKET))
            (if (and (= p 0)
                     (> point-u32 (load-val p)))
              (set! p (- (length BUCKET) 1))
              (while (and (< p (- (length BUCKET) 1))
                            (= (load-val p) point-u32))
                  (set! p (+ p 1)))))
          (insert-bucket-entry p (list point-u32 host port weight)))))))
