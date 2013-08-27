# Centralized autotools file
# To create the Visual C++ projects
# from the templates
# Author: Fan, Chun-wei
# August 30, 2012

# Required Items to call this:
# MSVC_PROJECT: name of project
# MSVC_PROJECT_SRCDIR: subdir of source tree where sources for this project is found
# MSVC_PROJECT_SRCS: source files to build
# MSVC_PROJECT_EXCLUDES: source files to exclude from MSVC_PROJECT_SRCS, use dummy if none,
# wildcards (*) are allowed, seperated by |
# DISTCLEANFILES: Define an empty one if not previously defined

# Create the complete Visual C++ 2008/2010 project files

$(top_builddir)/build/win32/vs9/$(MSVC_PROJECT).vcproj: $(top_srcdir)/build/win32/vs9/$(MSVC_PROJECT).vcprojin
	for F in `echo $(MSVC_PROJECT_SRCS) | sed 's/\.\///g' | tr '/' '\\'`; do \
		case $$F in \
			$(MSVC_PROJECT_EXCLUDES)) \
				;; \
			*.c) echo ' <File RelativePath="..\..\..\$(MSVC_PROJECT_SRCDIR)\'$$F'" />' \
				;; \
			esac; \
		done | sort -u >$(MSVC_PROJECT).sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/$(MSVC_PROJECT).vcprojin >$@
	rm $(MSVC_PROJECT).sourcefiles

$(top_builddir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj: $(top_srcdir)/build/win32/vs10/$(MSVC_PROJECT).vcxprojin
	for F in `echo $(MSVC_PROJECT_SRCS) | sed 's/\.\///g' | tr '/' '\\'`; do \
		case $$F in \
			$(MSVC_PROJECT_EXCLUDES)) \
				;; \
			*.c) echo ' <ClCompile Include="..\..\..\$(MSVC_PROJECT_SRCDIR)\'$$F'" />' \
				;; \
			esac; \
		done | sort -u >$(MSVC_PROJECT).vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/$(MSVC_PROJECT).vcxprojin >$@
	rm $(MSVC_PROJECT).vs10.sourcefiles

$(top_builddir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj.filters: $(top_srcdir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj.filtersin
	for F in `echo $(MSVC_PROJECT_SRCS) | sed 's/\.\///g' | tr '/' '\\'`; do \
		case $$F in \
			$(MSVC_PROJECT_EXCLUDES)) \
				;; \
			*.c) echo ' <ClCompile Include="..\..\..\$(MSVC_PROJECT_SRCDIR)\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
				;; \
			esac; \
		done | sort -u >$(MSVC_PROJECT).vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj.filtersin >$@
	rm $(MSVC_PROJECT).vs10.sourcefiles.filters

DISTCLEANFILES += \
	$(top_builddir)/build/win32/vs9/$(MSVC_PROJECT).vcproj \
	$(top_builddir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj \
	$(top_builddir)/build/win32/vs10/$(MSVC_PROJECT).vcxproj.filters
