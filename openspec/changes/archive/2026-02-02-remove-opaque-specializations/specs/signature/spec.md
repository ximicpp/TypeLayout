## REMOVED Requirements

### Requirement: TypeDiagnostic Utility
**Reason**: Diagnostic utilities are not layout facts; they are developer convenience tools.
**Migration**: Users should rely on compiler error messages for unsupported types.

### Requirement: Opaque Smart Pointer Signatures  
**Reason**: Hiding internal structure of std::unique_ptr, std::shared_ptr, std::weak_ptr is a semantic choice, not a layout fact.
**Migration**: Generic reflection engine will process these types and expose their actual implementation-defined layout.

### Requirement: Opaque Container Signatures
**Reason**: std::array, std::pair, std::span specializations provide "friendly names" rather than actual layout.
**Migration**: Generic reflection engine will process these types.

### Requirement: Opaque Atomic Signatures
**Reason**: std::atomic specialization hides implementation differences between libc++ and libstdc++.
**Migration**: Generic reflection engine will expose actual atomic layout.

### Requirement: Third-Party Type Support
**Reason**: boost::interprocess::offset_ptr is an external dependency and should not be in core.
**Migration**: None provided. Users can define their own specializations if needed.
