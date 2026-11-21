# @date 21st November, 2026

/*
 * ++ Pell Numbers ++
 *
 * Pell numbers are close cousin of Fibonacci
 * In fibonacci, its: fib(n) = fib(n - 1) + fib(n - 2)
 * In pell, its just: pell(n) = 2 * pell(n - 1) + pell(n - 2)
 * 
 * with pell(0) = 0, and pell(1) = 1
 */
 
# naive pell algorithm
# takes O(2^n) complexity
func pell_naive(index:int) {
    if index <= 1 {
        return index
    } else {
        return 2 * pell_naive(index - 1) + pell_naive(n - 2)
    }
}

# window sliding pell algorithm
# takes O(n) complexity
func pell_window(index:int) {
    if index <= 1 {
        return index
    } else {
        counter = 2
        a = 0
        b = 1
        c = 2
        while counter < index {
            a = c
            c = 2 * c + b
            b = a
            counter++
        }
        return c
    }
}

/*
 * ++ Collatz Conjecture ++
 *
 * Collatz series is defined as for any natural number n:
 *   
 *   f(n) = n / 2       ( if n even )
 *   f(n) = 3 * n + 1   ( if n odd)
 * 
 * Keep repeating this over & over, one will observe it inevitably ends in 1 (though none can prove it generally)
 */

 func collatz(num:int, printResult:bool = false) {
     steps = [num]
     while num != 1 {
         if num % 2 == 0 {
             num /= 2
             steps.push(num)
         } else {
             num = 3 * num + 1
             steps.push(num)
         }
     }
     if printResult {
         print("Number: ", num)
         print("Number of steps to reach 1: ", len(steps) - 1)
         print("Steps: ", steps)
     }
     return steps
 }

 print("Pell(5) = ", pell_naive(5))
 print("Pell(12) = ", pell_window(12))
 print(collatz(17))
 collatz(101, true)
