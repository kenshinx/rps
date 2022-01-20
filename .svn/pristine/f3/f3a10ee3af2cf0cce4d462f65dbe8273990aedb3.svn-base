%define debug_package %{nil}
%define __os_install_post %{nil}


%define approot  /home/netlab/serv/%{_module_name}
%define apipath  %{approot}/api/
%define binpath  %{approot}/bin/
%define confpath  %{approot}/conf/
%define logpath  %{approot}/logs/
%define scriptpath  %{approot}/script/
%define srcpath  %{approot}/src/
%define contribpath  %{approot}/contrib/

summary: The %{_module_name} project   
name: %{_module_name}
version: %{_version}
release: 1%{?dist}
license: Commercial
vendor: Qihoo <http://www.360.cn>
group: netlab/dev
source: %{_module_name}.tar.gz
#buildrequires: libicu-devel
buildroot: %{_tmppath}/%{name}-%{version}-%{release}-%(%{__id_u} -n)
Autoreq: no

%description
The %{_module_name} project to show how to write an nginx module

%prep
%setup -q -n %{_module_name}

%build
# your package build steps

#make DEBUG=" -DNDEBUG -O3 -D_USE_SEGMENTER" -j4
#make DEBUG=" -DNDEBUG -O3 " -j4

%install
# your package install steps
# the compiled files dir: %{_builddir}/<package_source_dir> or $RPM_BUILD_DIR/<package_source_dir>
# the dest root dir: %{buildroot} or $RPM_BUILD_ROOT

rm -rf %{buildroot}

mkdir -p %{buildroot}/%{approot}
mkdir -p %{buildroot}/%{apipath}
mkdir -p %{buildroot}/%{logpath}
mkdir -p %{buildroot}/%{confpath}
mkdir -p %{buildroot}/%{binpath}
mkdir -p %{buildroot}/%{scriptpath}
mkdir -p %{buildroot}/%{srcpath}
mkdir -p %{buildroot}/%{contribpath}

#echo buildroot: %{buildroot}

pushd %{_builddir}/%{_module_name}/script
cp -rf *  %{buildroot}/%{scriptpath}
popd


pushd %{_builddir}/%{_module_name}/conf
cp -rf *  %{buildroot}/%{confpath}
popd

pushd %{_builddir}/%{_module_name}/bin
cp -rf *  %{buildroot}/%{binpath}
popd

pushd %{_builddir}/%{_module_name}/api
cp -rf *  %{buildroot}/%{apipath}
popd

pushd %{_builddir}/%{_module_name}/src
cp -rf *  %{buildroot}/%{srcpath}
popd

pushd %{_builddir}/%{_module_name}/contrib
cp -rf *  %{buildroot}/%{contribpath}
popd

pushd %{_builddir}/%{_module_name}
cp -rf Makefile  %{buildroot}/%{approot}/
popd

#pushd %{_builddir}/%{_module_name}/logs
#cp -rf *  %{buildroot}/%{logpath}/
#popd

%files
# list your package files here
%defattr(-,netlab,netlab)

%dir %{approot}
%{approot}/Makefile

%dir %{apipath}
%{apipath}/*

%dir %{binpath}
%{binpath}/*

%dir %{confpath}
%{confpath}/*

%dir %{scriptpath}
%{scriptpath}/*

%dir %{srcpath}
%{srcpath}/*

%dir %{contribpath}
%{contribpath}/*

%dir %{logpath}


%pre
# pre-install scripts

%post

#build rps
	
cd %{approot} ; env CFLAGS="-DRPS_STACKTRACE -DX_FORWARD_PROXY" make noopt -B && make install

chown -R netlab:netlab /home/netlab/serv/%{_module_name}


%preun
# pre-uninstall scripts

%postun
# post-uninstall scripts

%clean
rm -rf %{buildroot}
# your package build clean up steps here

%changelog
# list your change log here


