{
  inputs.nixpkgs.url = "nixpkgs";
  inputs.bscpkgs.url = "git+https://git.sr.ht/~rodarima/bscpkgs";
  inputs.bscpkgs.inputs.nixpkgs.follows = "nixpkgs";

  nixConfig.bash-prompt = "\[nix-develop\]$ ";

  outputs = { self, nixpkgs, bscpkgs }:
  let
    # Set to true to replace all libovni in all runtimes with the current
    # source. Causes large rebuilds on changes of ovni.
    useLocalOvni = false;

    ovniOverlay = final: prev: {
      nosv = prev.nosv.override {
        useGit = true;
        gitBranch = "master";
        gitCommit = "23f83cb780dd8a705df60797c10d0b5fee28c527";
      };
      nanos6 = prev.nanos6.override {
        useGit = true;
        gitBranch = "master";
        gitCommit = "f39ea57c67a613d098050e2bb251116a021e91e5";
      };
      nodes = prev.nodes.override {
        useGit = true;
        gitBranch = "master";
        gitCommit = "c97d7ca6f885500121a94c75df429c788e8d6cf8";
      };
      clangOmpss2Unwrapped = prev.clangOmpss2Unwrapped.override {
        useGit = true;
        gitBranch = "master";
        gitCommit = "b7af30b36be3e7e90b33c5f01a3f7e3656df785f";
      };
      openmp = prev.openmp.overrideAttrs (old: {
        # Newer version of LLVM OpenMP requires python3
        nativeBuildInputs = (old.nativeBuildInputs or []) ++ [ final.python3 ];
      });

      # Use a fixed commit for libovni
      ovniFixed = prev.ovni.override {
        useGit = true;
        gitBranch = "master";
        gitCommit = "3bbfe0f0ecdf58e3f46ebafdf2540680f990b76b";
      };
      # Build with the current source
      ovniLocal = prev.ovni.overrideAttrs (old: rec {
        pname = "ovni-local";
        version = if self ? shortRev then self.shortRev else "dirty";
        src = self;
        cmakeFlags = [ "-DOVNI_GIT_COMMIT=${version}" ];
      });
      # Select correct ovni for libovni
      ovni = if (useLocalOvni) then final.ovniLocal else final.ovniFixed;
    };
    pkgs = import nixpkgs {
      system = "x86_64-linux";
      overlays = [
        bscpkgs.bscOverlay
        ovniOverlay
      ];
    };
    compilerList = with pkgs; [
      #gcc49Stdenv # broken
      gcc6Stdenv
      gcc7Stdenv
      gcc8Stdenv
      gcc9Stdenv
      gcc10Stdenv
      gcc11Stdenv
      gcc12Stdenv
      gcc13Stdenv
    ];
    lib = pkgs.lib;
  in {
    packages.x86_64-linux.ovniPackages = {
      # Allow inspection of packages from the command line
      inherit pkgs;
    } // rec {
      # Build with the current source
      local = pkgs.ovniLocal;

      # Build in Debug mode
      debug = local.overrideAttrs (old: {
        pname = "ovni-debug";
        cmakeBuildType = "Debug";
      });

      # Without MPI
      nompi = local.overrideAttrs (old: {
        pname = "ovni-nompi";
        buildInputs = lib.filter (x: x != pkgs.mpi ) old.buildInputs;
        cmakeFlags = old.cmakeFlags ++ [ "-DUSE_MPI=OFF" ];
      });

      # Helper function to build with different compilers
      genOvniCC = stdenv: (local.override {
        stdenv = stdenv;
      }).overrideAttrs (old: {
        name = "ovni-gcc" + stdenv.cc.cc.version;
        cmakeFlags = old.cmakeFlags ++ [
          "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF"
        ];
      });

      # Test several gcc versions
      compilers = let
        drvs = map genOvniCC compilerList;
      in pkgs.runCommand "ovni-compilers" { } ''
        printf "%s\n" ${builtins.toString drvs} > $out
      '';

      # Enable RT tests
      rt = (local.override {
        # Provide the clang compiler as default
        stdenv = pkgs.stdenvClangOmpss2;
      }).overrideAttrs (old: {
        pname = "ovni-rt";
        # We need to be able to exit the chroot to run Nanos6 tests, as they
        # require access to /sys for hwloc
        __noChroot = true;
        buildInputs = old.buildInputs ++ (with pkgs; [ pkg-config nosv nanos6 nodes openmpv ]);
        cmakeFlags = old.cmakeFlags ++ [ "-DENABLE_ALL_TESTS=ON" ];
        preConfigure = old.preConfigure or "" + ''
          export NOSV_HOME="${pkgs.nosv}"
          export NODES_HOME="${pkgs.nodes}"
          export NANOS6_HOME="${pkgs.nanos6}"
          export OPENMP_RUNTIME="libompv"
        '';
      });

      # Build with ASAN and pass RT tests
      asan = rt.overrideAttrs (old: {
        pname = "ovni-asan";
        cmakeFlags = old.cmakeFlags ++ [ "-DCMAKE_BUILD_TYPE=Asan" ];
        # Ignore leaks in tests for now, only check memory errors
        preCheck = old.preCheck + ''
          export ASAN_OPTIONS=detect_leaks=0
        '';
      });

      ubsan = rt.overrideAttrs (old: {
        pname = "ovni-ubsan";
        cmakeFlags = old.cmakeFlags ++ [ "-DCMAKE_BUILD_TYPE=Ubsan" ];
      });

      armv7 = (pkgs.pkgsCross.armv7l-hf-multiplatform.ovniLocal.overrideAttrs (old: {
        pname = "ovni-armv7";
        buildInputs = [];
        nativeBuildInputs = [ pkgs.pkgsCross.armv7l-hf-multiplatform.buildPackages.cmake ];
        cmakeFlags = old.cmakeFlags ++ [ "-DUSE_MPI=OFF" ];
      })).overrideDerivation (old: {
        doCheck = true;
      });

      aarch64 = (pkgs.pkgsCross.aarch64-multiplatform.ovniLocal.overrideAttrs (old: {
        pname = "ovni-aarch64";
        buildInputs = [];
        nativeBuildInputs = [ pkgs.pkgsCross.aarch64-multiplatform.buildPackages.cmake ];
        cmakeFlags = old.cmakeFlags ++ [ "-DUSE_MPI=OFF" ];
      })).overrideDerivation (old: {
        doCheck = true;
      });

      riscv64 = (pkgs.pkgsCross.riscv64.ovniLocal.overrideAttrs (old: {
        pname = "ovni-riscv64";
        buildInputs = [];
        nativeBuildInputs = [ pkgs.pkgsCross.riscv64.buildPackages.cmake ];
        cmakeFlags = old.cmakeFlags ++ [ "-DUSE_MPI=OFF" ];
      })).overrideDerivation (old: {
        doCheck = true;
      });
    };
  };
}
