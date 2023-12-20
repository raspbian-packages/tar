#!/bin/sh
#
#	clean upstream source to achieve DFSG compliance
#	Copyright 2009 by Bdale Garbee.  GPL v2 or any later version.
#

# preserve the man pages upstream has provided since 1.28
tmpfile=$(mktemp -p /tmp tar-doc.XXXXXXXX)
tar cf $tmpfile doc/tar.1 doc/rmt.8

git rm -rf doc
rm -rf doc

# This is a shell archive (produced by GNU sharutils 4.15.2).
# To extract the files from this archive, save it to some FILE, remove
# everything before the '#!/bin/sh' line above, then type 'sh FILE'.
#
lock_dir=_sh14383
# Made on 2016-07-22 13:04 MDT by <bdale@rover>.
# Source directory was '/home/bdale/debian/tar'.
#
# Existing files will *not* be overwritten, unless '-c' is specified.
#
# This shar contains:
# length mode       name
# ------ ---------- ------------------------------------------
#    638 -rw-r--r-- doc/README
#     51 -rw-r--r-- doc/Makefile.in
#     51 -rw-r--r-- doc/Makefile
#
MD5SUM=${MD5SUM-md5sum}
f=`${MD5SUM} --version | egrep '^md5sum .*(core|text)utils'`
test -n "${f}" && md5check=true || md5check=false
${md5check} || \
  echo 'Note: not verifying md5sums.  Consider installing GNU coreutils.'
if test "X$1" = "X-c"
then keep_file=''
else keep_file=true
fi
echo=echo
save_IFS="${IFS}"
IFS="${IFS}:"
gettext_dir=
locale_dir=
set_echo=false

for dir in $PATH
do
  if test -f $dir/gettext \
     && ($dir/gettext --version >/dev/null 2>&1)
  then
    case `$dir/gettext --version 2>&1 | sed 1q` in
      *GNU*) gettext_dir=$dir
      set_echo=true
      break ;;
    esac
  fi
done

if ${set_echo}
then
  set_echo=false
  for dir in $PATH
  do
    if test -f $dir/shar \
       && ($dir/shar --print-text-domain-dir >/dev/null 2>&1)
    then
      locale_dir=`$dir/shar --print-text-domain-dir`
      set_echo=true
      break
    fi
  done

  if ${set_echo}
  then
    TEXTDOMAINDIR=$locale_dir
    export TEXTDOMAINDIR
    TEXTDOMAIN=sharutils
    export TEXTDOMAIN
    echo="$gettext_dir/gettext -s"
  fi
fi
IFS="$save_IFS"
if (echo "testing\c"; echo 1,2,3) | grep c >/dev/null
then if (echo -n test; echo 1,2,3) | grep n >/dev/null
     then shar_n= shar_c='
'
     else shar_n=-n shar_c= ; fi
else shar_n= shar_c='\c' ; fi
f=shar-touch.$$
st1=200112312359.59
st2=123123592001.59
st2tr=123123592001.5 # old SysV 14-char limit
st3=1231235901

if   touch -am -t ${st1} ${f} >/dev/null 2>&1 && \
     test ! -f ${st1} && test -f ${f}; then
  shar_touch='touch -am -t $1$2$3$4$5$6.$7 "$8"'

elif touch -am ${st2} ${f} >/dev/null 2>&1 && \
     test ! -f ${st2} && test ! -f ${st2tr} && test -f ${f}; then
  shar_touch='touch -am $3$4$5$6$1$2.$7 "$8"'

elif touch -am ${st3} ${f} >/dev/null 2>&1 && \
     test ! -f ${st3} && test -f ${f}; then
  shar_touch='touch -am $3$4$5$6$2 "$8"'

else
  shar_touch=:
  echo
  ${echo} 'WARNING: not restoring timestamps.  Consider getting and
installing GNU '\''touch'\'', distributed in GNU coreutils...'
  echo
fi
rm -f ${st1} ${st2} ${st2tr} ${st3} ${f}
#
if test ! -d ${lock_dir} ; then :
else ${echo} "lock directory ${lock_dir} exists"
     exit 1
fi
if mkdir ${lock_dir}
then ${echo} "x - created lock directory ${lock_dir}."
else ${echo} "x - failed to create lock directory ${lock_dir}."
     exit 1
fi
# ============= doc/README ==============
if test ! -d 'doc'; then
  mkdir 'doc'
if test $? -eq 0
then ${echo} "x - created directory doc."
else ${echo} "x - failed to create directory doc."
     exit 1
fi
fi
if test -n "${keep_file}" && test -f 'doc/README'
then
${echo} "x - SKIPPING doc/README (file already exists)"

else
${echo} "x - extracting doc/README (text)"
  sed 's/^X//' << 'SHAR_EOF' > 'doc/README' &&
Unfortunately, the info document describing tar is licensed under the GFDL with
invariant cover texts, which violates the Debian Free Software Guidelines.  As
a result, the info documentation for tar has been completely removed from the
Debian package.  Fortunately, since 1.28 the upstream tar package includes
man page sources, so at least that documentation is included in the Debian
main tar package.
X
If you want to read the complete info documentation for GNU tar, please either
install the 'tar-doc' package from Debian's non-free repository, or refer to
the online version at <http://www.gnu.org/software/tar/manual/index.html>.
X
SHAR_EOF
  (set 20 16 07 22 13 04 11 'doc/README'
   eval "${shar_touch}") && \
  chmod 0644 'doc/README'
if test $? -ne 0
then ${echo} "restore of doc/README failed"
fi
  if ${md5check}
  then (
       ${MD5SUM} -c >/dev/null 2>&1 || ${echo} 'doc/README': 'MD5 check failed'
       ) << \SHAR_EOF
2f70316837b4f65972a24e61ab8f0897  doc/README
SHAR_EOF

else
test `LC_ALL=C wc -c < 'doc/README'` -ne 638 && \
  ${echo} "restoration warning:  size of 'doc/README' is not 638"
  fi
fi
# ============= doc/Makefile.in ==============
if test ! -d 'doc'; then
  mkdir 'doc'
if test $? -eq 0
then ${echo} "x - created directory doc."
else ${echo} "x - failed to create directory doc."
     exit 1
fi
fi
if test -n "${keep_file}" && test -f 'doc/Makefile.in'
then
${echo} "x - SKIPPING doc/Makefile.in (file already exists)"

else
${echo} "x - extracting doc/Makefile.in (text)"
  sed 's/^X//' << 'SHAR_EOF' > 'doc/Makefile.in' &&
all:
X
install:
X
check:
X
distclean:
X	rm -f Makefile
SHAR_EOF
  (set 20 16 07 22 13 03 35 'doc/Makefile.in'
   eval "${shar_touch}") && \
  chmod 0644 'doc/Makefile.in'
if test $? -ne 0
then ${echo} "restore of doc/Makefile.in failed"
fi
  if ${md5check}
  then (
       ${MD5SUM} -c >/dev/null 2>&1 || ${echo} 'doc/Makefile.in': 'MD5 check failed'
       ) << \SHAR_EOF
426042328bcada50997fe11fff91ca61  doc/Makefile.in
SHAR_EOF

else
test `LC_ALL=C wc -c < 'doc/Makefile.in'` -ne 51 && \
  ${echo} "restoration warning:  size of 'doc/Makefile.in' is not 51"
  fi
fi
# ============= doc/Makefile ==============
if test -n "${keep_file}" && test -f 'doc/Makefile'
then
${echo} "x - SKIPPING doc/Makefile (file already exists)"

else
${echo} "x - extracting doc/Makefile (text)"
  sed 's/^X//' << 'SHAR_EOF' > 'doc/Makefile' &&
all:
X
install:
X
check:
X
distclean:
X	rm -f Makefile
SHAR_EOF
  (set 20 16 07 22 13 03 35 'doc/Makefile'
   eval "${shar_touch}") && \
  chmod 0644 'doc/Makefile'
if test $? -ne 0
then ${echo} "restore of doc/Makefile failed"
fi
  if ${md5check}
  then (
       ${MD5SUM} -c >/dev/null 2>&1 || ${echo} 'doc/Makefile': 'MD5 check failed'
       ) << \SHAR_EOF
426042328bcada50997fe11fff91ca61  doc/Makefile
SHAR_EOF

else
test `LC_ALL=C wc -c < 'doc/Makefile'` -ne 51 && \
  ${echo} "restoration warning:  size of 'doc/Makefile' is not 51"
  fi
fi
if rm -fr ${lock_dir}
then ${echo} "x - removed lock directory ${lock_dir}."
else ${echo} "x - failed to remove lock directory ${lock_dir}."
     exit 1
fi

tar xf $tmpfile doc/tar.1 doc/rmt.8
git add doc/Makefile doc/Makefile.in doc/README doc/tar.1 doc/rmt.8

exit 0

