
{
  inputs = {
    nixpkgs = {
      url = "github:nixos/nixpkgs/nixos-unstable";
    };
    flake-utils = {
      url = "github:numtide/flake-utils";
    };

  };
  outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      # Use clang+mold
      stdenv = pkgs.stdenvAdapters.useMoldLinker pkgs.clangStdenv;
      
      pkgs = import nixpkgs {
        inherit system;
        overlays = [
          (final: prev: {
            backward-cpp = prev.backward-cpp.overrideAttrs(old: {
              buildInputs = [ prev.cmake ];
              installPhase = null;
              cmakeFlags = [ "-DBACKWARD_TESTS=OFF"];
            });
          })
        ];
      };

      inja = (with pkgs; stdenv.mkDerivation {
          pname = "inja";
          version = "3.4.0";
          src = fetchgit {
            url = "https://github.com/pantor/inja";
            rev = "v3.4.0";
            sha256 = "B1EaR+qN32nLm3rdnlZvXQ/dlSd5XSc+5+gzBTPzUZU=";
            fetchSubmodules = true;
          };
          buildInputs = [
            cmake
            nlohmann_json
          ];
          cmakeFlags = [
            "-DBUILD_BENCHMARK=OFF"
            "-DINJA_BUILD_TESTS=OFF"
            "-DINJA_USE_EMBEDDED_JSON=OFF"
          ];
          buildPhase = "make -j $NIX_BUILD_CORES";
         }
      );

      mcap = (with pkgs; stdenv.mkDerivation {
          pname = "mcap";
          version = "3.4.0";
          src = fetchgit {
            url = "https://github.com/foxglove/mcap";
            rev = "releases/cpp/v1.4.1";
            sha256 = "pvmQtuE8sgCrHBSB8sgGduKwZezuMzP50Fz9vs6XmVM=";
            fetchSubmodules = true;
          };
          installPhase = ''          
              runHook preInstall
              cp -r cpp/mcap/include $out/
              runHook postInstall
          '';
         }
      );
    in rec {

      asio_noboost = (with pkgs; stdenv.mkDerivation {
        pname = "asio";
        version = "1.32.0-noboost";
        src = fetchgit {
          url = "https://github.com/chriskohlhoff/asio/";
          rev = "asio-1-32-0";
          sha256 = "PBoa4OAOOmHas9wCutjz80rWXc3zGONntb9vTQk3FNY=";
        };
        preConfigure = ''
          runHook preInstall
          cp -r asio/include $out/
          runHook postInstall
        '';
      });


      foxglove_websocket = (with pkgs; stdenv.mkDerivation rec {
        pname = "foxglove_websocket";
        version = "1.3.0";
        src = fetchgit {
          url = "https://github.com/foxglove/ws-protocol/";
          rev = "releases/cpp/v${version}";
          sha256 = "JEsTIBdLMGoehscbPyr3KgE05+eDb8Lj/+bwMLALQ6I=";
        };
        buildInputs = [ cmake ];
        propagatedBuildInputs = [
          asio_noboost
          openssl
          websocketpp
          nlohmann_json
        ];
        # It's really hard to get this propagated via regular patches, somehow
        postPatch = ''
echo "#define ASIO_STANDALONE
$(catcpp/foxglove-websocket/src/server_factory.cpp)" > cpp/foxglove-websocket/src/server_factory.cpp
'';
        preConfigure = "cd cpp/foxglove-websocket";
      });

      foxglove_schemas = (with pkgs; stdenv.mkDerivation rec {
        pname = "foxglove_schemas";
        version = "0.2.2";
        src = fetchgit {
          url = "https://github.com/foxglove/schemas";
          rev = "releases/python/foxglove-schemas-protobuf/v${version}";
          sha256 = "3DAhLxjr4sfNfxWj+TpoztPZjVWrGHNfbbtwzoPS+L8=";
        };
        installPhase = ''          
            runHook preInstall
            cp -r . $out/
            runHook postInstall
        '';
      });

      basis = (with pkgs; stdenv.mkDerivation {
          pname = "basis";
          version = "0.0.8";
          src = self;
          buildInputs = [
            argparse
            backward-cpp
            elfutils
            clang
            cmake
            yaml-cpp
            expected-lite
            inja
            mcap
            protobuf
            foxglove_websocket
            foxglove_schemas
            # only if testing
            gtest
          ];
          cmakeFlags = [
            "-DMCAP_INCLUDE_DIRECTORY=\"${mcap.outPath}\""
          ];
          buildPhase = "make -j $NIX_BUILD_CORES";
        }
      );
      defaultApp = flake-utils.lib.mkApp {
        drv = defaultPackage;
      };
      defaultPackage = basis;
      devShell = (with pkgs; mkShell {
        buildInputs = [
          basis.buildInputs
          git
          # ugh, this isn't working
          lesspipe
          coreutils
        ];
      });
    }
  );
}