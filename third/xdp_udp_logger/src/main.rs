use redbpf_probes::xdp::prelude::*;

#[xdp]
pub fn xdp_udp_logger(ctx: XdpContext) -> XdpResult {
    if let Ok(transport) = ctx.transport() {
        if let Transport::UDP(udp) = transport {
            if let Ok(ip) = ctx.ip() {
                println!(
                    "<{}>:<{}> => <{}>:<{}>",
                    ip.source(),
                    udp.source(),
                    ip.destination(),
                    udp.destination()
                );
            }
        }
    }

    Ok(XdpAction::Pass)
}
