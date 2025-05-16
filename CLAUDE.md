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
