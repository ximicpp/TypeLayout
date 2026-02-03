/**
 * TypeLayout Video Scene Configuration
 * 
 * Each scene defines:
 * - id: Unique identifier
 * - duration: Duration in seconds
 * - narration: Text for TTS
 * - visual: Description of what to show (for manual reference)
 * - slideIndex: Which slide to capture (if using slides)
 */

module.exports = {
  // Video metadata
  title: "TypeLayout: Compile-Time Binary Compatibility for C++26",
  totalDuration: 345, // 5:45 in seconds
  resolution: { width: 1920, height: 1080 },
  fps: 30,
  
  // TTS configuration
  tts: {
    voice: "en-US-GuyNeural",  // Male, professional
    rate: "+0%",
    pitch: "+0Hz"
  },
  
  // Scene definitions
  scenes: [
    // ============ Scene 1: Hook (0:00 - 0:30) ============
    {
      id: "hook-1",
      duration: 5,
      slideIndex: 4, // "A Simple Struct" slide
      narration: "How big is this struct?",
      pauseAfter: 1
    },
    {
      id: "hook-2", 
      duration: 8,
      slideIndex: 4,
      narration: "If you said 13 bytes, you just introduced a production bug."
    },
    {
      id: "hook-3",
      duration: 10,
      slideIndex: 5, // "The Reality" slide
      narration: "It's actually 24 bytes. 9 bytes of hidden padding. And the compiler won't warn you."
    },
    
    // ============ Scene 2: Problem (0:30 - 1:30) ============
    {
      id: "problem-1",
      duration: 8,
      slideIndex: 7, // "The Silent Bug" slide
      narration: "Binary compatibility bugs are among the most insidious in C++."
    },
    {
      id: "problem-2",
      duration: 12,
      slideIndex: 7,
      narration: "Look at this example. Same source code, but 16 bytes on Linux, 12 bytes on Windows."
    },
    {
      id: "problem-3",
      duration: 12,
      slideIndex: 7,
      narration: "When they communicate over the network, data gets corrupted. No compiler error, no runtime exception, just silent data corruption."
    },
    {
      id: "problem-4",
      duration: 10,
      slideIndex: 6, // "When Does This Matter" slide
      narration: "This happens in network protocols, shared memory, file formats, hardware register mapping."
    },
    {
      id: "problem-5",
      duration: 10,
      slideIndex: 8, // "It Gets Worse" slide
      narration: "And it gets worse with struct versioning. Old binaries reading new data leads to crashes."
    },
    
    // ============ Scene 3: Traditional Solutions (1:30 - 2:15) ============
    {
      id: "traditional-1",
      duration: 10,
      slideIndex: 10, // "Manual Verification" slide
      narration: "Traditional solutions have significant tradeoffs. Manual static asserts are easy to forget and can't access private members."
    },
    {
      id: "traditional-2",
      duration: 10,
      slideIndex: 11, // "Serialization" slide
      narration: "Serialization libraries like Protocol Buffers add runtime overhead and require external IDL files."
    },
    {
      id: "traditional-3",
      duration: 10,
      slideIndex: 12, // "Pragma Pack" slide
      narration: "Pragma pack causes performance penalties and isn't portable across all platforms."
    },
    {
      id: "traditional-4",
      duration: 8,
      slideIndex: 14, // "What We Really Need" slide
      narration: "What we really need is compile-time verification, zero runtime overhead, and human-readable diagnostics."
    },
    
    // ============ Scene 4: TypeLayout Intro (2:15 - 3:15) ============
    {
      id: "typelayout-1",
      duration: 8,
      slideIndex: 16, // "The Vision" slide
      narration: "TypeLayout solves this with a single line of code."
    },
    {
      id: "typelayout-2",
      duration: 12,
      slideIndex: 16,
      narration: "It generates a complete memory layout signature at compile time, including every field's offset and type."
    },
    {
      id: "typelayout-3",
      duration: 10,
      slideIndex: 19, // "When Verification Fails" slide
      narration: "If the layout doesn't match, compilation fails. Not a runtime crash, a compile-time error."
    },
    {
      id: "typelayout-4",
      duration: 8,
      slideIndex: 17, // "Reading a Signature" slide
      narration: "Zero runtime overhead. Everything happens at compile time."
    },
    
    // ============ Scene 5: Signature Format (3:15 - 3:45) ============
    {
      id: "signature-1",
      duration: 10,
      slideIndex: 17,
      narration: "The signature format is designed to be human-readable. Square brackets show total size and alignment."
    },
    {
      id: "signature-2",
      duration: 10,
      slideIndex: 18, // "Signature Elements" slide
      narration: "Each field shows its offset, name, and type. You can debug layout issues directly from log output."
    },
    
    // ============ Scene 6: P2996 (3:45 - 4:30) ============
    {
      id: "p2996-1",
      duration: 8,
      slideIndex: 21, // "C++26 Static Reflection" slide
      narration: "How is this possible? The answer is C++26 static reflection, proposal P 29 96."
    },
    {
      id: "p2996-2",
      duration: 12,
      slideIndex: 22, // "Reflection Basics" slide
      narration: "Use the caret operator to get compile-time reflection info. Then iterate over all members to get names, offsets, and types."
    },
    {
      id: "p2996-3",
      duration: 12,
      slideIndex: 24, // "Building the Signature" slide
      narration: "The key point: this all happens at compile time. No runtime reflection, no RTTI overhead."
    },
    {
      id: "p2996-4",
      duration: 8,
      slideIndex: 26, // "Why P2996" slide
      narration: "TypeLayout is one of the first practical applications of P 29 96."
    },
    
    // ============ Scene 7: Applications (4:30 - 5:15) ============
    {
      id: "apps-1",
      duration: 10,
      slideIndex: 28, // "Shared Memory IPC" slide
      narration: "TypeLayout works for any scenario requiring precise memory layout. Shared memory IPC between processes."
    },
    {
      id: "apps-2",
      duration: 8,
      slideIndex: 29, // "Network Protocols" slide
      narration: "Network protocols where clients and servers must agree on packet format."
    },
    {
      id: "apps-3",
      duration: 8,
      slideIndex: 30, // "Hardware Registers" slide
      narration: "Embedded development where hardware register mapping must be byte-accurate."
    },
    {
      id: "apps-4",
      duration: 10,
      slideIndex: 31, // "CI/CD" slide
      narration: "Even CI/CD integration, catching ABI breaks automatically before they reach production."
    },
    
    // ============ Scene 8: Conclusion (5:15 - 5:45) ============
    {
      id: "conclusion-1",
      duration: 12,
      slideIndex: 33, // "Summary" slide
      narration: "TypeLayout: compile-time binary compatibility verification. Zero overhead, human-readable, C++26 standard compliant."
    },
    {
      id: "conclusion-2",
      duration: 10,
      slideIndex: 34, // "Try It Today" slide
      narration: "Visit the GitHub repository for source code and the complete tutorial."
    },
    {
      id: "conclusion-3",
      duration: 5,
      slideIndex: 36, // "Questions" slide
      narration: "Thanks for watching."
    }
  ]
};
