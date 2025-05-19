# JSONdb Development Guidelines

- Do not make minimal implementations. Delete partial concept files and ideas. Focus on:
  1. One source of truth
  2. One build (always result is bin/jsondb_server)
  3. One clear goal
  4. Build with full functionality
  5. Always fix, never regress
  6. Document thoroughly
  7. Repeat the improvement cycle
  8. One Makefile for the project src/Makefile
  9. One main.c for the project src/components/main.c
 10. Components are in src/components
 11. Headers are in src/include
 12. We have impeccable git hygiene!
 13. Guidelines are in docs/guidelines, read them
 14. ONLY BUILD USING THE MAKEFILE in src/ directory
 15. DO NOT run binaries from the build directory, only interact with the server using the script in build we created
 16. Server runs on port 5000 by default
 17. DO NOT IMPLEMENT MOCK DATA OR DEMO MODE - ALWAYS WORK WITH REAL SERVER DATA
 18. Maintain zero-warning policy - always compile with -Wall -Wextra
 19. Use proper string handling to prevent buffer overflows
 20. Document all fixes thoroughly for future reference
 21. Never create minimal server tests, always work on the main code
 22. Always build from src using make, and run from build using jsondb_runtime.sh, no exceptions.
 23. Always follow our git hygiene guidelines.
 24. Only invoke the server using the runtime script in build, never directly except if explicitly asked.
 25. Never assume anything is broken on the host device or platform.  This is a VM that's tried and tested, and supports multiple project developement in parallel.