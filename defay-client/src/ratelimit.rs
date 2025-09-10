use std::time::{Duration, Instant};
use parking_lot::Mutex;

/// Simple token-bucket rate limiter.
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
    /// Create a new bucket with given burst capacity and refill window.
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

    /// Attempt to consume one token. Returns false if bucket is empty.
    pub fn try_take(&self) -> bool {
        let mut t = self.tokens.lock();
        let now = Instant::now();
        // refill if necessary
        if now.duration_since(t.last_refill) >= self.refill_every {
            t.available = self.capacity;
            t.last_refill = now;
        }
        if t.available > 0 {
            t.available -= 1;
            true
        } else {
            false
        }
    }

    /// Seconds until next token is available.
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

    /// Number of available tokens remaining.
    pub fn available(&self) -> u32 {
        self.tokens.lock().available
    }
}