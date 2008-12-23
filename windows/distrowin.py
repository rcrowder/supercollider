# distrowin.py
# script to generate SuperCollider WIX (windows installer xml) source file from template
# (c) Dan Stowell 2008, published under the GPL v2 or later

# REQUIREMENTS: 
#  (1) You must have installed the "WiX toolset" and added its "bin" folder to your PATH
#  (2) You must have run the Visual Studio compilation process to create Psycollider stuff in the "build" folder
#  (3) This script file, and the wix template, must be in the "windows" folder in SuperCollider3 svn tree (sibling to the "build" folder)
#  (4) I think you also need to put FFTW and libsndfile DLLs into the "build" folder

import os, glob, uuid, re, sys, shutil, zipfile

########################################
# Check for SwingOSC, because we definitely want it included :)
for detectpath in ('../build/SwingOSC.jar', '../build/SCClassLibrary/SwingOSC', '../build/Help/SwingOSC'):
        if not os.path.exists(detectpath):
                print("ERROR:\n  Path %s not detected.\n  It's required for bundling SwingOSC into the distro." % detectpath)
                sys.exit(1)

########################################
# Run the "py2exe" procedure to build an exe file

os.system('cd ../Psycollider/Psycollider && python setup.py py2exe')
for detectpath in ('../Psycollider/Psycollider/dist/Psycollider.exe', '../Psycollider/Psycollider/dist/w9xpopen.exe'):
        if not os.path.exists(detectpath):
                print("ERROR:\n  Path %s not detected.\n  Generating executable (using py2exe) probably failed." % detectpath)
                sys.exit(1)
# Also copy PySCLang.pyd out of its "site-packages" location
shutil.copy(os.getenv('PYTHONPATH', sys.exec_prefix) + '/Lib/site-packages/PySCLang.pyd', '../Psycollider/Psycollider/dist/')
# and a dll we need
shutil.copy(os.getenv('PYTHONPATH', sys.exec_prefix) + '/Lib/site-packages/wx-2.8-msw-unicode/wx/gdiplus.dll', '../Psycollider/Psycollider/dist/')


########################################
# Now we start to build up the XML content for WiX
xmlstr1 = ""  # directory tree
xmlstr2 = ""  # "feature" contents

regex1 = re.compile('[^a-zA-Z0-9.]')
def pathToId(path):
	global regex1
	id = regex1.sub('_', path.replace('../build/', ''))
	return id[max(len(id)-65, 0):]
def pathToGuid(path):
	return str(uuid.uuid3(uuid.NAMESPACE_DNS, 'supercollider.sourceforge.net/' + path))

# This recursively scans a directory and builds up the requisite XML for installing the relevant files.
def scanDirForWix(path, fileexts, nestlev):
	global xmlstr1, xmlstr2
	dircontents = os.listdir(path)
	for item in dircontents:
		fullerpath = path + '/' + item
		if os.path.isdir(fullerpath) and item[0] != '.' and item!='osx' and item!='linux': 	# the '.' is to exclude .svn
			# print fullerpath
			xmlstr1 = xmlstr1 + '  '*nestlev + '<Directory Id="%s" Name="%s">\n' % (pathToId(fullerpath), item)
			# Recurse:
			scanDirForWix(fullerpath, fileexts, nestlev+1)
			xmlstr1 = xmlstr1 + '  '*nestlev + '</Directory>\n'
		elif os.path.isfile(fullerpath) and not os.path.islink(fullerpath):
			for fileext in fileexts:
				if item.lower().endswith(fileexts): #and item matches a certain range of file extensions:
					# print fullerpath + " --- FILE"
					compId = pathToId(fullerpath)
					xmlstr1 = xmlstr1 + '  '*nestlev + '<Component Id="%s" Guid="%s">\n' % (compId, pathToGuid(fullerpath))
					xmlstr1 = xmlstr1 + '  '*nestlev + '    <File Id="%s" Name="%s" Source="%s" DiskId="1"/>\n' % \
										(compId + '.file', item, fullerpath)
					xmlstr1 = xmlstr1 + '  '*nestlev + '</Component>\n'
					xmlstr2 = xmlstr2 + '    <ComponentRef Id="%s" />\n' % compId
					break
		#else:
		#	print 'Ignored %s\n' % fullerpath

# Now we do all the different scans we want

xmlstr1 = xmlstr1 + '<DirectoryRef Id="SCpluginsFolder">\n'
xmlstr2 = xmlstr2 + '<Feature Id="CorePluginsFeature" Title="Server plugins" Description="Core set of SC3 plugins" Level="1">\n'
scanDirForWix('../build/plugins', ('.scx'), 0)
xmlstr2 = xmlstr2 + '</Feature>\n'
xmlstr1 = xmlstr1 + '</DirectoryRef>\n\n'

xmlstr1 = xmlstr1 + '<DirectoryRef Id="SCHelpFolder">\n'
xmlstr2 = xmlstr2 + '<Feature Id="HelpFilesFeature" Title="Help files" Description="SC3 help documentation" Level="1">\n'
scanDirForWix('../build/Help', ('.html', '.htm', '.rtf', '.rtfd', '.jpg', '.png', '.gif', '.scd'), 0)
xmlstr2 = xmlstr2 + '</Feature>\n'
xmlstr1 = xmlstr1 + '</DirectoryRef>\n\n'

xmlstr1 = xmlstr1 + '<DirectoryRef Id="SCsoundsFolder">\n'
xmlstr2 = xmlstr2 + '<Feature Id="SoundFilesFeature" Title="Sound files" Description="Some audio files" Level="1">\n'
scanDirForWix("../build/sounds", (".aiff", ".wav", ".aif"), 0)
xmlstr2 = xmlstr2 + '</Feature>\n'
xmlstr1 = xmlstr1 + '</DirectoryRef>\n\n'

xmlstr1 = xmlstr1 + '<DirectoryRef Id="SCClassLibrary">\n'
xmlstr2 = xmlstr2 + '<Feature Id="SCClassLibraryFeature" Title="SC3 class files" Description="The classes which define the SuperCollider language" Level="1">\n'
scanDirForWix("../build/SCClassLibrary", (".sc"), 0)
xmlstr2 = xmlstr2 + '</Feature>\n'
xmlstr1 = xmlstr1 + '</DirectoryRef>\n\n'


# WORKAROUND FOR M$ BUG:
# Windows installer is supposed to be able to handle massive numbers of files, but actually it fucks up if a <Feature> contains more than around 1000 files.
# See http://www.add-in-express.com/creating-addins-blog/2007/11/12/windows-installer-error-2908/
# Because of this, we need to artificially split the helpfiles feature in two.
xmlstr2b = xmlstr2.split('<ComponentRef Id="Help_Style_Guide', 1)
if not len(xmlstr2b) == 2:
	print "Warning, unable to break up the XML string as expected."
else:
	xmlstr2 = xmlstr2b[0] + '</Feature>\n<Feature Id="HelpFilesFeaturePT2" Title="Help files, part 2" Description="SC3 help documentation" Level="1">\n<ComponentRef Id="Help_Style_Guide' + xmlstr2b[1]

# OK, now we have the XML fragments, we want to substitute them into the XML template file
template = ''
templatef = open('sc3-win-installer-template.wxs', 'r')
for line in templatef:
	template = template + line
templatef.close()

template = template.split('<!-- SUBST:SPLITHERE -->');

f = open('supercollider-installer.wxs', 'w')
f.write(template[0])
f.write(xmlstr1)
f.write(template[1])
f.write(xmlstr2)
f.write(template[2])
f.close()

print "\ndistrowin.py: done generating WiX file\n"

print "Calling WiX compile/link steps...\n"
os.system('candle supercollider-installer.wxs')
os.system('light -ext WixUIExtension -cultures:en-us supercollider-installer.wixobj')

print "\ndistrowin.py: done building MSI\n"
print "\ndistrowin.py: now to bundle a zip file\n"

z = zipfile.ZipFile('supercollider-installer.zip', 'w', zipfile.ZIP_DEFLATED)
z.write('supercollider-installer.msi')
z.write('INSTALL.txt')
z.write('../COPYING', 'COPYING.txt')
z.write('copyright_info_forwinbundle.txt', 'copyright info.txt')
z.close()


print "\ndistrowin.py: done\n"
