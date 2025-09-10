use parking_lot::Mutex;
use std::time::{Duration, Instant};

pub struct TokenBucket {
    capacity: u32,
    tokens: Mutex<Tokens>,
    refill_every: Duration,
    refill_amount: u32,
}

struct Tokens {
    available: u32,
    last_refill: Instant,
}

impl TokenBucket {
    pub fn new(burst: u32, window: Duration) -> Self {
        Self {
            capacity: burst,
            tokens: Mutex::new(Tokens {
                available: burst,
                last_refill: Instant::now(),
            }),
            refill_every: window,
            refill_amount: burst,
        }
    }

    pub fn try_take(&self) -> bool {
        let mut t = self.tokens.lock();
        let now = Instant::now();

        let elapsed = now.duration_since(t.last_refill);
        if elapsed >= self.refill_every {
            let intervals =
                (elapsed.as_secs_f64() / self.refill_every.as_secs_f64()).floor() as u32;
            if intervals > 0 {
                let added = intervals.saturating_mul(self.refill_amount);
                t.available = (t.available + added).min(self.capacity);
                t.last_refill += self.refill_every * intervals;
            }
        }

        if t.available > 0 {
            t.available -= 1;
            true
        } else {
            false
        }
    }

    pub fn seconds_to_next(&self) -> u64 {
        let t = self.tokens.lock();
        let now = Instant::now();
        if t.available > 0 {
            0
        } else {
            let elapsed = now.duration_since(t.last_refill);
            if elapsed >= self.refill_every {
                0
            } else {
                (self.refill_every - elapsed).as_secs()
            }
        }
    }

    pub fn available(&self) -> u32 {
        self.tokens.lock().available
    }
}
