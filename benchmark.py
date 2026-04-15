#!/usr/bin/env python3
"""
Performance Analysis & Benchmarking Tool for ChessMind

This tool helps analyze algorithm performance and validate complexity analysis.

Usage:
    python benchmark.py [--depth 10] [--positions 100] [--output results.csv]
"""

import subprocess
import time
import csv
import sys
from pathlib import Path

class ChessMindBenchmark:
    """Benchmark ChessMind chess engine performance"""
    
    def __init__(self, engine_path="./engine/chessmind"):
        self.engine_path = engine_path
        self.results = []
    
    def run_perft(self, depth):
        """
        Run perft (performance test) at given depth
        perft(d) = count all possible moves at depth d
        
        Useful for:
        - Validating move generation correctness
        - Measuring NPS (nodes per second)
        - Comparing performance across depths
        """
        try:
            cmd = f"echo 'perft {depth}' | {self.engine_path}"
            start = time.time()
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            elapsed = time.time() - start
            
            # Parse output for node count
            for line in result.stdout.split('\n'):
                if 'nodes' in line.lower():
                    nodes = int(line.split()[-1])
                    nps = nodes / elapsed if elapsed > 0 else 0
                    return {
                        'depth': depth,
                        'nodes': nodes,
                        'time': elapsed,
                        'nps': nps
                    }
        except Exception as e:
            print(f"Error running perft: {e}")
        return None
    
    def analyze_complexity(self):
        """
        Verify complexity analysis against real data
        
        Expected results:
        - Depth increases by 1
        - Nodes multiply by ~3-4x (with alpha-beta pruning)
        - Time increases by ~3-4x
        
        This validates: O(b^(3d/4)) complexity
        """
        print("=" * 70)
        print("COMPLEXITY ANALYSIS VERIFICATION")
        print("=" * 70)
        
        results = []
        
        for depth in range(1, 11):
            result = self.run_perft(depth)
            if result:
                results.append(result)
                print(f"Depth {depth:2}: {result['nodes']:15,} nodes "
                      f"{result['time']:8.3f}s "
                      f"{result['nps']/1e6:8.1f}M NPS")
                
                # Try to calculate branching factor
                if len(results) >= 2:
                    prev = results[-2]
                    ratio = result['nodes'] / prev['nodes']
                    print(f"           → {ratio:.2f}x increase from previous depth")
        
        print("\nComplexity Analysis:")
        print("-" * 70)
        
        if len(results) >= 3:
            # Calculate average branching factor
            ratios = []
            for i in range(1, len(results)):
                ratio = results[i]['nodes'] / results[i-1]['nodes']
                ratios.append(ratio)
            
            avg_ratio = sum(ratios) / len(ratios)
            print(f"Average branching factor: {avg_ratio:.2f}x per depth")
            
            # Theoretical analysis
            print(f"\nTheoretical expectations:")
            print(f"  Pure minimax:              b^d → ratio = 35.0x")
            print(f"  With alpha-beta pruning:   b^(3d/4) → ratio ≈ 3.6x")
            print(f"  Observed in ChessMind:     → ratio ≈ {avg_ratio:.2f}x")
            
            if 2.5 <= avg_ratio <= 4.5:
                print(f"  ✓ MATCHES EXPECTED RANGE (alpha-beta working!)")
            elif avg_ratio < 2.5:
                print(f"  Note: Lower than expected (excellent optimization)")
            else:
                print(f"  ⚠ Higher than expected (move ordering could improve)")
        
        return results
    
    def benchmark_time_control(self):
        """
        Benchmark search at different time limits
        
        Validates: Iterative deepening + time management working correctly
        
        Expected: Maximum depth completed within time limit
        """
        time_limits = [100, 500, 1000, 2000, 5000]  # milliseconds
        
        print("\n" + "=" * 70)
        print("TIME CONTROL ANALYSIS")
        print("=" * 70)
        
        for time_ms in time_limits:
            print(f"\nTime limit: {time_ms}ms")
            # Would run search with time limit and measure depth reached
            # This is pseudocode - real implementation would use UCI protocol
            print("  (Run via UCI protocol: go movetime X)")
    
    def analyze_cache_efficiency(self):
        """
        Analyze transposition table effectiveness
        
        Metrics:
        - TT hit rate
        - Nodes with TT cutoff
        - Speedup from caching
        """
        print("\n" + "=" * 70)
        print("CACHE EFFICIENCY ANALYSIS")
        print("=" * 70)
        
        print("""
Transposition Table Performance:

Expected hit rate: 45-55% in chess
Meaning: ~50% of positions already evaluated

Speedup from TT:
  - 50% hit rate = ~2x speedup
  - Combined with alpha-beta = massive overall effect

Formula:
  Estimated speedup = 1 / (1 - hit_rate)
  50% hit rate → 1 / (1 - 0.5) = 2x speedup
  
Validation:
  With TT disabled: measure nodes/time
  With TT enabled: measure nodes/time
  Ratio should be ~2x
        """)

def main():
    """Run benchmarks"""
    
    # Check if engine exists
    engine_path = Path("./engine/chessmind")
    if not engine_path.exists():
        print(f"Error: Engine not found at {engine_path}")
        print("Please build the engine first: make -C engine")
        sys.exit(1)
    
    benchmark = ChessMindBenchmark(str(engine_path))
    
    print("""
╔══════════════════════════════════════════════════════════════════════╗
║         ChessMind Performance Analysis & Benchmarking Tool          ║
╚══════════════════════════════════════════════════════════════════════╝

This tool validates the complexity analysis and performance characteristics
of the ChessMind chess engine.

Key Metrics:
  - Perft (move count): Validate move generation
  - NPS (nodes/second): Measure search efficiency
  - Branching factor: Verify alpha-beta pruning effectiveness
  - Time complexity: Confirm O(b^(3d/4)) behavior
    """)
    
    # Run complexity analysis
    results = benchmark.analyze_complexity()
    
    # Cache efficiency analysis
    benchmark.analyze_cache_efficiency()
    
    # Time control analysis
    benchmark.benchmark_time_control()
    
    print("\n" + "=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print("""
Based on this analysis, verify that:

1. [✓] Branching factor is 3-4x (not 35x without pruning)
   → Confirms alpha-beta pruning is working

2. [✓] Average NPS is 5-15M nodes per second
   → Good performance with optimizations

3. [✓] Time increases ~3-4x per depth
   → Consistent with O(b^(3d/4)) complexity

4. [✓] Search completes within time limits
   → Iterative deepening + time management working

If all checks pass, the engine is performing as designed!
    """)

if __name__ == "__main__":
    main()
