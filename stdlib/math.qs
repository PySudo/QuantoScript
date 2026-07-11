# Math helpers.

func min(items:list) {
    best = items[1]
    repeat item -> items {
        if item < best {
            best = item
        }
    }
    return best
}

func max(items:list) {
    best = items[1]
    repeat item -> items {
        if item > best {
            best = item
        }
    }
    return best
}

func clamp(value:number, low:number, high:number) {
    if value < low {
        return low
    }
    if value > high {
        return high
    }
    return value
}
