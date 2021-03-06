################################################################################
sub PACKAGE      { "sra-tools" }
sub VERSION      { "2.4.2-1" }
sub PACKAGE_TYPE { 'B' }
sub PACKAGE_NAME { "SRA-TOOLS" }
sub PACKAGE_NAMW { "SRATOOLS" }
sub CONFIG_OUT   { 'build' }
sub PKG { ( LNG   => 'C',
            OUT   => 'ncbi-outdir',
            PATH  => '/usr/local/ncbi/sra-tools',
            UPATH =>      '$HOME/ncbi/sra-tools', ) }
sub DEPENDS      { ( { name => 'hdf5' , },
                     { name => 'magic', Include => '/usr/include'        , },
                     { name => 'xml2' , Include => '/usr/include/libxml2', } ) }
sub REQ { (
            { name    => 'ngs-sdk',
              namew   => 'NGS',
              option  => 'with-ngs-sdk-prefix',
              origin  => 'I',
              type    => 'L',
              srcpath => '../ngs/ngs-sdk',
              pkgpath => '/usr/local/ngs/ngs-sdk',
              usrpath =>      '$HOME/ngs/ngs-sdk',
              bldpath => '$HOME/ncbi-outdir/ngs-sdk',
              include => 'ngs/itf/Refcount.h',
              lib     => 'libngs-c++.a',
              ilib    => 'libngs-bind-c++.a',
            },
            { name    => 'ncbi-vdb',
              namew   => 'VDB',
              option  => 'with-ncbi-vdb-sources',
              boption => 'with-ncbi-vdb-build',
              origin  => 'I',
              type    => 'SB',
              srcpath => '../ncbi-vdb',
              pkgpath => '/usr/local/ncbi/ncbi-vdb',
              usrpath =>      '$HOME/ncbi/ncbi-vdb',
              bldpath => '$HOME/ncbi-outdir/ncbi-vdb',
              include => 'klib/rc.h',
              lib     => 'libncbi-vdb.a',
              ilib    => 'libkapp.a',
        },
        {     name    => 'hdf5',
              option  => 'with-hdf5-prefix',
              origin  => 'E',
              type    => 'LI',
              pkgpath => '/usr',
              usrpath => '$HOME',
              include => 'hdf5.h',
              lib => 'libhdf5.a',
        } ) }
1
