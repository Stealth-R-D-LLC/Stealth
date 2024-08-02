#######################################################################
##  MacOS Include Makefile for Tor
#######################################################################


TOR_INCLUDEPATHS = \
  -I"$(CURDIR)/tor" \
  -I"$(CURDIR)/tor/adapter"

TOR_CFLAGS = $(CFLAGS) $(TOR_INCLUDEPATHS) $(DEBUGFLAGS)

TOR_DEFS = $(addprefix -I, $(OPENSSL_INCLUDE_PATH) \
                           $(EVENT_INCLUDE_PATH))

OBJS += \
  obj/circuitpadding_machines.o \
  obj/scheduler_kist.o \
  obj/periodic.o \
  obj/onion_ntor_v3.o \
  obj/circuitlist.o \
  obj/conflux_params.o \
  obj/conflux_pool.o \
  obj/trace_probes_circuit.o \
  obj/extendinfo.o \
  obj/hs_ntor.o \
  obj/circuitpadding.o \
  obj/proto_haproxy.o \
  obj/channelpadding.o \
  obj/crypt_path.o \
  obj/channeltls.o \
  obj/congestion_control_flow.o \
  obj/scheduler_vanilla.o \
  obj/onion.o \
  obj/conflux.o \
  obj/reasons.o \
  obj/command.o \
  obj/connection_edge.o \
  obj/circuituse.o \
  obj/address_set.o \
  obj/circuitmux_ewma.o \
  obj/netstatus.o \
  obj/circuitbuild.o \
  obj/conflux_cell.o \
  obj/orconn_event.o \
  obj/dos.o \
  obj/onion_tap.o \
  obj/protover.o \
  obj/trace_probes_cc.o \
  obj/dos_config.o \
  obj/channel.o \
  obj/mainloop.o \
  obj/dos_sys.o \
  obj/mainloop_sys.o \
  obj/onion_fast.o \
  obj/conflux_sys.o \
  obj/ocirc_event.o \
  obj/circuitmux.o \
  obj/conflux_util.o \
  obj/sendme.o \
  obj/proto_socks.o \
  obj/proto_ext_or.o \
  obj/onion_ntor.o \
  obj/connection.o \
  obj/connection_or.o \
  obj/relay_crypto.o \
  obj/proto_cell.o \
  obj/status.o \
  obj/circuitstats.o \
  obj/congestion_control_common.o \
  obj/cpuworker.o \
  obj/or_sys.o \
  obj/policies.o \
  obj/proto_http.o \
  obj/scheduler.o \
  obj/or_periodic.o \
  obj/proto_control0.o \
  obj/congestion_control_vegas.o \
  obj/relay.o \
  obj/onion_crypto.o \
  obj/mainloop_pubsub.o \
  obj/versions.o \
  obj/quiet_level.o \
  obj/subsystem_list.o \
  obj/tor_main.o \
  obj/subsysmgr.o \
  obj/ntmain.o \
  obj/shutdown.o \
  obj/resolve_addr.o \
  obj/config.o \
  obj/statefile.o \
  obj/toronion_main.o \
  obj/risky_options.o \
  obj/readpassphrase.o \
  obj/csiphash.o \
  obj/keccak-tiny-unrolled.o \
  obj/blake2.o \
  obj/siphash_rng.o \
  obj/compiler_a64.o \
  obj/program.o \
  obj/hashx_context.o \
  obj/compiler_x86.o \
  obj/program_exec.o \
  obj/hashx.o \
  obj/compiler.o \
  obj/virtual_memory.o \
  obj/siphash.o \
  obj/equix.o \
  obj/context.o \
  obj/solver.o \
  obj/trunnel.o \
  obj/curve25519-donna.o \
  obj/link_handshake.o \
  obj/ed25519_cert.o \
  obj/extension.o \
  obj/cell_establish_intro.o \
  obj/cell_rendezvous.o \
  obj/cell_introduce1.o \
  obj/socks5.o \
  obj/flow_control_cells.o \
  obj/sendme_cell.o \
  obj/congestion_control.o \
  obj/circpad_negotiation.o \
  obj/trunnel_conflux.o \
  obj/netinfo.o \
  obj/pwbox.o \
  obj/channelpadding_negotiation.o \
  obj/parse_int.o \
  obj/procmon.o \
  obj/prob_distr.o \
  obj/crypto_ope.o \
  obj/log.o \
  obj/memarea.o \
  obj/crypto_dh.o \
  obj/printf.o \
  obj/aes_openssl.o \
  obj/order.o \
  obj/trace.o \
  obj/fdio.o \
  obj/log_sys.o \
  obj/process_sys.o \
  obj/crypto_format.o \
  obj/scanf.o \
  obj/pubsub_check.o \
  obj/tvdiff.o \
  obj/compat_libevent.o \
  obj/tortls.o \
  obj/namemap.o \
  obj/process.o \
  obj/smartlist_split.o \
  obj/fp.o \
  obj/compat_time.o \
  obj/crypto_rsa.o \
  obj/tor_gettimeofday.o \
  obj/structvar.o \
  obj/conffile.o \
  obj/torerr.o \
  obj/escape.o \
  obj/compress_zlib.o \
  obj/compress.o \
  obj/toronion_version.o \
  obj/gethostname.o \
  obj/metrics_common.o \
  obj/network_sys.o \
  obj/mmap.o \
  obj/crypto_pwbox.o \
  obj/compress_zstd.o \
  obj/files.o \
  obj/map_anon.o \
  obj/libc.o \
  obj/torerr_sys.o \
  obj/dispatch_new.o \
  obj/winprocess_sys.o \
  obj/resolve.o \
  obj/numcpus.o \
  obj/crypto_hkdf.o \
  obj/compat_pthreads.o \
  obj/storagedir.o \
  obj/time_to_tm.o \
  obj/digestset.o \
  obj/time_sys.o \
  obj/dispatch_naming.o \
  obj/metrics_store.o \
  obj/freespace.o \
  obj/bits.o \
  obj/crypto_digest_openssl.o \
  obj/socket.o \
  obj/crypto_s2k.o \
  obj/sandbox.o \
  obj/di_ops.o \
  obj/dir.o \
  obj/muldiv.o \
  obj/buffers.o \
  obj/pubsub_publish.o \
  obj/ratelim.o \
  obj/qstring.o \
  obj/compat_ctype.o \
  obj/binascii.o \
  obj/getpass.o \
  obj/crypto_digest.o \
  obj/pubsub_build.o \
  obj/setuid.o \
  obj/kvline.o \
  obj/approx_time.o \
  obj/addsub.o \
  obj/token_bucket.o \
  obj/compat_threads.o \
  obj/inaddr.o \
  obj/pidfile.o \
  obj/confmgt.o \
  obj/buffers_tls.o \
  obj/crypto_rand.o \
  obj/env.o \
  obj/crypto_dh_openssl.o \
  obj/pem.o \
  obj/timers.o \
  obj/buffers_net.o \
  obj/git_revision.o \
  obj/crypto_util.o \
  obj/compress_lzma.o \
  obj/backtrace.o \
  obj/dispatch_core.o \
  obj/typedvar.o \
  obj/waitpid.o \
  obj/daemon.o \
  obj/confline.o \
  obj/util_bug.o \
  obj/keyval.o \
  obj/time_fmt.o \
  obj/path.o \
  obj/crypto_ed25519.o \
  obj/unitparse.o \
  obj/map.o \
  obj/meminfo.o \
  obj/bloomfilt.o \
  obj/prometheus.o \
  obj/laplace.o \
  obj/compress_none.o \
  obj/uname.o \
  obj/compat_mutex_pthreads.o \
  obj/crypto_rand_fast.o \
  obj/x509.o \
  obj/crypto_cipher.o \
  obj/malloc.o \
  obj/socketpair.o \
  obj/crypto_rand_numeric.o \
  obj/evloop_sys.o \
  obj/workqueue.o \
  obj/lockfile.o \
  obj/smartlist.o \
  obj/process_unix.o \
  obj/address.o \
  obj/util_string.o \
  obj/restrict.o \
  obj/geoip.o \
  obj/smartlist_core.o \
  obj/userdb.o \
  obj/type_defs.o \
  obj/trace_sys.o \
  obj/weakrng.o \
  obj/x509_openssl.o \
  obj/cstring.o \
  obj/alertsock.o \
  obj/dispatch_cfg.o \
  obj/crypto_openssl_mgt.o \
  obj/crypto_rsa_openssl.o \
  obj/crypto_curve25519.o \
  obj/compat_mutex.o \
  obj/tortls_openssl.o \
  obj/crypto_init.o \
  obj/compress_buf.o \
  obj/compat_string.o \
  obj/metrics_store_entry.o \
  obj/onion_queue.o \
  obj/rephist.o \
  obj/hs_dos.o \
  obj/voting_schedule.o \
  obj/control_hs.o \
  obj/fmt_routerstatus.o \
  obj/process_descs.o \
  obj/hs_pow.o \
  obj/circuitbuild_relay.o \
  obj/directory.o \
  obj/reachability.o \
  obj/hs_control.o \
  obj/hs_ob.o \
  obj/routerparse.o \
  obj/consdiff.o \
  obj/microdesc_parse.o \
  obj/dsigs_parse.o \
  obj/ns_parse.o \
  obj/relay_find_addr.o \
  obj/authcert.o \
  obj/tor_api.o \
  obj/hs_descriptor.o \
  obj/hs_circuit.o \
  obj/hs_service.o \
  obj/authmode.o \
  obj/control_bootstrap.o \
  obj/dirauth_config.o \
  obj/getinfo_geoip.o \
  obj/transport_config.o \
  obj/shared_random.o \
  obj/rendmid.o \
  obj/conscache.o \
  obj/shared_random_state.o \
  obj/dnsserv.o \
  obj/geoip_stats.o \
  obj/authcert_parse.o \
  obj/dns.o \
  obj/hs_metrics_entry.o \
  obj/router.o \
  obj/hs_config.o \
  obj/control_events.o \
  obj/bwauth.o \
  obj/torcert.o \
  obj/routerkeys.o \
  obj/nodefamily.o \
  obj/proxymode.o \
  obj/networkstatus.o \
  obj/dircache.o \
  obj/signing.o \
  obj/btrack_circuit.o \
  obj/unparseable.o \
  obj/routerset.o \
  obj/bridgeauth.o \
  obj/hs_sys.o \
  obj/metrics.o \
  obj/hs_circuitmap.o \
  obj/guardfraction.o \
  obj/hs_client.o \
  obj/dirauth_periodic.o \
  obj/control_auth.o \
  obj/control_cmd.o \
  obj/btrack_orconn_maps.o \
  obj/relay_metrics.o \
  obj/rendcommon.o \
  obj/hs_cache.o \
  obj/hs_cell.o \
  obj/btrack.o \
  obj/connstats.o \
  obj/hs_intropoint.o \
  obj/control.o \
  obj/dirauth_sys.o \
  obj/nodelist.o \
  obj/routerinfo.o \
  obj/hibernate.o \
  obj/hs_ident.o \
  obj/dirvote.o \
  obj/relay_periodic.o \
  obj/relay_handshake.o \
  obj/keypin.o \
  obj/btrack_orconn.o \
  obj/policy_parse.o \
  obj/entrynodes.o \
  obj/nickname.o \
  obj/ext_orport.o \
  obj/fp_pair.o \
  obj/selftest.o \
  obj/dircollate.o \
  obj/consdiffmgr.o \
  obj/bridges.o \
  obj/control_proto.o \
  obj/voteflags.o \
  obj/predict_ports.o \
  obj/circpathbias.o \
  obj/microdesc.o \
  obj/hs_stats.o \
  obj/node_select.o \
  obj/relay_sys.o \
  obj/routermode.o \
  obj/hs_metrics.o \
  obj/dirlist.o \
  obj/dirserv.o \
  obj/addressmap.o \
  obj/routerlist.o \
  obj/replaycache.o \
  obj/dlstatus.o \
  obj/relay_config.o \
  obj/describe.o \
  obj/transports.o \
  obj/hs_common.o \
  obj/recommend_pkg.o \
  obj/metrics_sys.o \
  obj/sigcommon.o \
  obj/loadkey.o \
  obj/btrack_orconn_cevent.o \
  obj/parsecommon.o \
  obj/shared_random_client.o \
  obj/dirclient.o \
  obj/bwhist.o \
  obj/control_fmt.o \
  obj/control_getinfo.o \
  obj/dirclient_modes.o

obj/%.o: tor/adapter/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/core/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/app/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/keccak-tiny/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/equix/hashx/src/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/equix/src/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/trunnel/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/ext/curve25519_donna/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/trunnel/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/lib/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

obj/%.o: tor/feature/%.c
	$(CC) -c $(TOR_DEFS) $(TOR_CFLAGS) -MMD -MF $(@:%.o=%.d) -o $@ $<
	@cp $(@:%.o=%.d) $(@:%.o=%.P); \
	  sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	      -e '/^$$/ d' -e 's/$$/ :/' < $(@:%.o=%.d) >> $(@:%.o=%.P); \
	  rm -f $(@:%.o=%.d)

