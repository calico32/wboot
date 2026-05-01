#import "@preview/touying:0.7.3": *
#import "libtailwind.typ": tw


#let title-slide(..args) = touying-slide-wrapper(self => {
    self = utils.merge-dicts(
        self,
        config-page(
            margin: (x: 2.5cm, y: 0cm),
        ),
    )
    let info = self.info + args.named()
    let body = {
        set align(horizon)
        set par(leading: 0.5em)
        block(
            width: 90%,
            below: 1.5em,
            text(
                info.title,
                size: 2em,
                weight: "bold",
                font: "Inter Display",
                fill: self.colors.primary-light,
            ),
        )
        if info.subtitle != none {
            block(
                width: 90%,
                below: 3em,
                text(
                    info.subtitle,
                    size: 1.5em,
                    style: "italic",
                    weight: 500,
                    font: "Inter Display",
                    fill: self.colors.primary-lightest,
                ),
            )
        }
        set text(fill: self.colors.neutral-dark, size: .8em)
        if info.author != none {
            block(
                below: 1em,
                info.author,
            )
        }
        if info.institution != none {
            block(
                below: 1em,
                info.institution,
            )
        }
        if info.date != none {
            block(
                below: 1em,
                utils.display-info-date(self),
            )
        }
        if info.contact != none {
            block(
                below: 1em,
                info.contact,
            )
        }
    }
    touying-slide(self: self, body)
})

#let new-section-slide(self: none, body) = touying-slide-wrapper(self => {
    let main-body = {
        set align(center + horizon)
        set text(
            size: 2em,
            fill: self.colors.primary-light,
            weight: "bold",
            font: "Inter Display",
        )
        utils.display-current-heading(level: 1)
    }
    touying-slide(self: self, main-body)
})

#let slide(title: auto, ..args) = touying-slide-wrapper(self => {
    // set page
    let header(self) = {
        set align(top)
        show: components.cell.with(fill: self.colors.primary, inset: 1em)
        set align(horizon)
        set text(
            font: "Inter Display",
            fill: self.colors.neutral-lightest,
        )
        text(
            size: .6em,
            tracking: .1em,
            weight: "bold",
            font: "Inter Display",
            fill: self.colors.neutral-lightest,
            upper(utils.display-current-heading(level: 1)),
        )
        linebreak()
        text(
            size: 1.3em,
            weight: "semibold",
            font: "Inter Display",
            fill: self.colors.neutral-lightest,
            utils.display-current-heading(level: 2),
        )
    }
    let footer(self) = context {
        set align(bottom)
        utils.touying-progress(ratio => {
            // omit title slide in progress calculation
            let slides = utils.last-slide-counter.final().at(0)
            let ratio = (ratio * slides - 1) / (slides - 1)
            // ratio is a float between 0.0 and 1.0
            box(width: ratio * 100%, height: 4pt, fill: self.colors.primary)
        })
    }
    self = utils.merge-dicts(
        self,
        config-page(
            header: header,
            footer: footer,
        ),
    )
    touying-slide(self: self, ..args)
})

#let wboot-theme(
    aspect-ratio: "16-9",
    footer: "wboot",
    ..args,
    body,
) = {
    set text(size: 20pt, fill: tw.zinc.s300)
    set list(spacing: 1em)

    show: touying-slides.with(
        config-page(
            paper: "presentation-" + aspect-ratio,
            margin: (top: 5em, bottom: 1em, x: 1.5em),
            fill: tw.zinc.s900,
        ),
        config-common(
            slide-fn: slide,
            // new-section-slide-fn: new-section-slide,
        ),
        config-methods(
            alert: (self: none, body) => text(
                fill: self.colors.primary-light,
                body,
            ),
        ),
        config-colors(
            primary-lightest: tw.indigo.s300,
            primary-lighter: tw.indigo.s400,
            primary-light: tw.indigo.s500,
            primary: tw.indigo.s600,
            primary-dark: tw.indigo.s700,
            primary-darker: tw.indigo.s800,
            primary-darkest: tw.indigo.s900,

            secondary-lightest: tw.fuchsia.s200,
            secondary-lighter: tw.fuchsia.s300,
            secondary-light: tw.fuchsia.s400,
            secondary: tw.fuchsia.s500,
            secondary-dark: tw.fuchsia.s600,
            secondary-darker: tw.fuchsia.s700,
            secondary-darkest: tw.fuchsia.s800,

            neutral-lightest: tw.zinc.s50,
            neutral-lighter: tw.zinc.s100,
            neutral-light: tw.zinc.s200,
            neutral: tw.zinc.s300,
            neutral-dark: tw.zinc.s400,
            neutral-darker: tw.zinc.s500,
            neutral-darkest: tw.zinc.s600,
        ),
        config-store(
            title: none,
            footer: footer,
        ),
        ..args,
    )

    body
}
