# Dogecoin Core - Systematic Rebranding Plan

## Executive Summary
Transform Dogecoin Core from Bitcoin-named conventions to true Dogecoin identity, maintaining all functionality while establishing unique branding.

## Phase 1: Critical File Renaming (High Priority)

### A. Main Application Files
- `src/bitcoind.cpp` → `src/dogecoind.cpp`
- `src/bitcoin-cli.cpp` → `src/dogecoin-cli.cpp` 
- `src/bitcoin-tx.cpp` → `src/dogecoin-tx.cpp`
- `src/qt/bitcoin.cpp` → `src/qt/dogecoin.cpp`

### B. GUI Core Files
- `src/qt/bitcoingui.h` → `src/qt/dogecoingui.h`
- `src/qt/bitcoingui.cpp` → `src/qt/dogecoingui.cpp`

### C. Configuration Files
- `src/config/bitcoin-config.h` → `src/config/dogecoin-config.h`
- `configure.ac` (update references)
- `src/Makefile.am` (update references)

## Phase 2: Class and Namespace Renaming

### A. Core Classes
- `BitcoinGUI` → `DogecoinGUI`
- `BitcoinApplication` → `DogecoinApplication`
- `BitcoinUnits` → `DogecoinUnits`
- `BitcoinAddressValidator` → `DogecoinAddressValidator`
- `BitcoinAmountField` → `DogecoinAmountField`

### B. Build System Macros
- `BITCOIN_CONFIG_INCLUDES` → `DOGECOIN_CONFIG_INCLUDES`
- `BITCOIN_INCLUDES` → `DOGECOIN_INCLUDES`
- `BITCOIN_CORE_H` → `DOGECOIN_CORE_H`
- All `BITCOIN_*` macros → `DOGECOIN_*`

## Phase 3: String and Text Updates

### A. User-Facing Strings
- "Bitcoin Core" → "Dogecoin Core"
- "Bitcoin" → "Dogecoin" (in UI text)
- Package names and descriptions
- Help text and documentation

### B. Internal Comments
- Update all code comments
- Update copyright notices where appropriate
- Update documentation strings

## Phase 4: Build System Updates

### A. Makefile Updates
- Update all library names
- Update executable names
- Update include paths
- Update dependency references

### B. Configuration Updates
- Update `configure.ac` references
- Update build scripts
- Update package configurations

## Phase 5: Documentation Updates

### A. Man Pages
- Update all manual pages
- Update help text
- Update examples

### B. README Files
- Update project descriptions
- Update build instructions
- Update contribution guidelines

## Implementation Strategy

### Step 1: Backup and Preparation
1. Create full backup of current codebase
2. Create feature branch for rebranding
3. Document current state

### Step 2: File Renaming (Automated)
1. Use `git mv` for file renames to preserve history
2. Update all include statements
3. Update all references in build files

### Step 3: Class Renaming (Manual)
1. Update class declarations in headers
2. Update class definitions in source files
3. Update all instantiations and references

### Step 4: Build System Updates
1. Update Makefile.am files
2. Update configure.ac
3. Test compilation at each step

### Step 5: Testing and Validation
1. Ensure all themes still work
2. Verify all functionality preserved
3. Test on multiple platforms

## Risk Mitigation

### A. Compilation Risks
- Test compilation after each major change
- Keep original files as backup until fully tested
- Use incremental approach

### B. Functionality Risks
- Maintain all existing APIs
- Preserve all wallet functionality
- Keep theme system intact

### C. User Experience Risks
- Maintain familiar UI patterns
- Keep all features accessible
- Preserve configuration compatibility

## Success Criteria

### A. Technical Success
- [ ] All files compile successfully
- [ ] All themes work correctly
- [ ] All wallet functions work
- [ ] All RPC commands work
- [ ] Cross-platform compatibility maintained

### B. Branding Success
- [ ] No "Bitcoin" references in user-facing text
- [ ] Consistent "Dogecoin" branding throughout
- [ ] Professional appearance maintained
- [ ] Clear Dogecoin identity established

### C. Quality Success
- [ ] No regressions in functionality
- [ ] Performance maintained or improved
- [ ] Code quality preserved
- [ ] Documentation updated

## Timeline Estimate

- **Phase 1 (File Renaming)**: 2-3 days
- **Phase 2 (Class Renaming)**: 3-4 days  
- **Phase 3 (String Updates)**: 1-2 days
- **Phase 4 (Build System)**: 2-3 days
- **Phase 5 (Documentation)**: 1-2 days
- **Testing & Validation**: 2-3 days

**Total Estimated Time**: 11-17 days

## Next Steps

1. **Start with Phase 1** - Critical file renaming
2. **Test compilation** after each major change
3. **Maintain functionality** throughout process
4. **Document changes** for future reference
5. **Celebrate** the transformation! 🐕✨

This systematic approach ensures we transform Dogecoin Core into its own unique identity while maintaining all the sophisticated functionality we've built.
